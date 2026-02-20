#include "mpi_merge.hpp"
#include <mpi.h>
#include <omp.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

#include "index.hpp"
#include "io.hpp"
#include "utils.hpp"

// Crea un MPI_Datatype che descrive la struct IndexRec in memoria
static MPI_Datatype make_mpi_indexrec_type() {
    MPI_Datatype t;
    IndexRec dummy{};
    // indirizzo base
    MPI_Aint base;
    MPI_Get_address(&dummy, &base);

    // displacement dei 3 campi
    MPI_Aint displs[3];
    MPI_Get_address(&dummy.key, &displs[0]);
    MPI_Get_address(&dummy.offset, &displs[1]);
    MPI_Get_address(&dummy.len, &displs[2]);
    // relativamente al base
    for (int i = 0; i < 3; ++i) displs[i] -= base;

    // un elemento per campo
    int bl[3] = {1, 1, 1};

    // tipi corrispondenti
    MPI_Datatype types[3];
    types[0] = MPI_UNSIGNED_LONG;
    types[1] = MPI_UINT64_T;
    types[2] = MPI_UINT32_T;

    MPI_Type_create_struct(3, bl, displs, types, &t);
    MPI_Type_commit(&t);
    return t;
}

// Merge a 2 vie di due vettori già ordinati per key (IndexRec)
static std::vector<IndexRec> merge_two_sorted(const std::vector<IndexRec>& a,
                                              const std::vector<IndexRec>& b) {
    std::vector<IndexRec> out;
    out.reserve(a.size() + b.size());
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].key <= b[j].key) out.push_back(a[i++]);
        else out.push_back(b[j++]);
    }
    while (i < a.size()) out.push_back(a[i++]);
    while (j < b.size()) out.push_back(b[j++]);
    return out;
}

// merge a k vie di P segmenti ordinati (rimane utile se vuoi fare test su root)
// all_sorted_segments = concatenazione dei segmenti già ordinati internamente
// counts[s] = lunghezza del segmento s
// displs[s] = offset di inizio s dentro all_sorted_segments
static std::vector<IndexRec> kway_merge(const std::vector<IndexRec>& all_sorted_segments,
                                       const std::vector<int>& counts,
                                       const std::vector<int>& displs) {
    const int P = (int)counts.size();

    struct Node {
        IndexRec rec;      // record corrente
        int src;           // quale segmento
        int pos_in_src;    // quale posizione
    };

    // priority_queue è max-heap -> invertiamo per ottenere min-heap
    auto cmp = [](const Node& a, const Node& b) {
        return index_less(b.rec, a.rec);
    };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> pq(cmp);

    // init heap con il primo el di ogni segmento non vuoto
    for (int s = 0; s < P; ++s) {
        if (counts[s] > 0) {
            const IndexRec& r0 = all_sorted_segments[(std::size_t)displs[s]];
            pq.push(Node{r0, s, 0});
        }
    }

    std::vector<IndexRec> out;
    out.reserve(all_sorted_segments.size());

    while (!pq.empty()) {
        Node n = pq.top();
        pq.pop();
        out.push_back(n.rec);

        const int s = n.src;
        const int next_pos = n.pos_in_src + 1;
        if (next_pos < counts[s]) {
            const IndexRec& rn =
                all_sorted_segments[(std::size_t)displs[s] + (std::size_t)next_pos];
            pq.push(Node{rn, s, next_pos});
        }
    }
    return out;
}

// MPI + OMP
/*
 * - rank 0 genera dataset e costruisce indice globale
 * - rank 0 divide l'indice in slice contigue e le distribuisce
 * - ogni rank ordina la propria slice usando OMP
 * - merge globale DISTRIBUITO con tree-merge (no gather su root)
 * - rank 0 riscrive il file ordinato e verifica
 */
int run_mpi(const Params& p) {
    int rank = 0, P = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    MPI_Datatype MPI_INDEXREC = make_mpi_indexrec_type();

    // tempi
    double t_total0 = MPI_Wtime();
    double t_build = 0.0, t_sort = 0.0, t_merge = 0.0, t_write = 0.0, t_check = 0.0;

    std::string in_path;
    std::vector<IndexRec> global_idx;

    // rank0 genera file e costruisce indice
    if (rank == 0) {
        GenStats st{};
        in_path = ensure_unsorted_file(p.n_records, p.payload_max, &st);

        double t0 = MPI_Wtime();
        global_idx = build_index_mmap(in_path, p.n_records);
        t_build = MPI_Wtime() - t0;
    }

    // broadcast della lunghezza path e contenuto a tutti i rank
    int path_len = 0;
    if (rank == 0) path_len = (int)in_path.size();
    MPI_Bcast(&path_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) in_path.resize((std::size_t)path_len);
    MPI_Bcast(in_path.data(), path_len, MPI_CHAR, 0, MPI_COMM_WORLD);

    // rank0 calcola counts/displs per slice contigue (bilanciamento per #record)
    std::vector<int> counts, displs;
    if (rank == 0) {
        counts.assign(P, 0);
        displs.assign(P, 0);

        const std::size_t N = p.n_records;
        for (int r = 0; r < P; ++r) {
            std::size_t start = (std::uint64_t(r) * N) / (std::uint64_t)P;
            std::size_t end = (std::uint64_t(r + 1) * N) / (std::uint64_t)P;
            counts[r] = (int)(end - start);
        }
        for (int r = 1; r < P; ++r) displs[r] = displs[r - 1] + counts[r - 1];

        // check coerenza
        if ((std::size_t)(displs.back() + counts.back()) != N) {
            MPI_Type_free(&MPI_INDEXREC);
            throw std::runtime_error("Internal error: counts/displs do not sum to N");
        }
    }

    // scatter: invio a ogni rank quanti elementi riceverà
    int recvcount = 0;
    MPI_Scatter(rank == 0 ? counts.data() : nullptr, 1, MPI_INT,
                &recvcount, 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<IndexRec> local_idx;
    local_idx.resize((std::size_t)recvcount);

    // invio delle slice di IndexRec
    MPI_Scatterv(rank == 0 ? global_idx.data() : nullptr,
                 rank == 0 ? counts.data() : nullptr,
                 rank == 0 ? displs.data() : nullptr,
                 MPI_INDEXREC,
                 local_idx.data(), recvcount, MPI_INDEXREC,
                 0, MPI_COMM_WORLD);

    // barriera per rendere il timing comparabile
    MPI_Barrier(MPI_COMM_WORLD);

    // sort locale (OMP)
    double t0s = MPI_Wtime();
    if (p.n_threads > 0) omp_set_num_threads((int)p.n_threads);
    mergesort_index_openmp(local_idx, p.cutoff);
    t_sort = MPI_Wtime() - t0s;

    // tutti devono essere ordinati
    int ok_local = is_sorted_by_key(local_idx) ? 1 : 0;
    int ok_all = 0;
    MPI_Allreduce(&ok_local, &ok_all, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!ok_all) {
        if (rank == 0) std::cerr << "[error] At least one rank produced a non-sorted slice\n";
        MPI_Type_free(&MPI_INDEXREC);
        return 1;
    }

    // MERGE GLOBALE DISTRIBUITO: tree-merge logaritmico 
    MPI_Barrier(MPI_COMM_WORLD);
    double t0m = MPI_Wtime();

    std::vector<IndexRec> work = std::move(local_idx);

    for (int step = 1; step < P; step *= 2) {
        if ((rank % (2 * step)) == 0) {
            int partner = rank + step;
            if (partner < P) {
                int recv_n = 0;
                MPI_Recv(&recv_n, 1, MPI_INT, partner, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                std::vector<IndexRec> other((std::size_t)recv_n);
                MPI_Recv(other.data(), recv_n, MPI_INDEXREC, partner, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                work = merge_two_sorted(work, other);
            }
        } else {
            int partner = rank - step;

            int send_n = (int)work.size();
            MPI_Send(&send_n, 1, MPI_INT, partner, 100, MPI_COMM_WORLD);
            MPI_Send(work.data(), send_n, MPI_INDEXREC, partner, 101, MPI_COMM_WORLD);

            // questo rank esce dal merge
            break;
        }
    }

    t_merge = MPI_Wtime() - t0m;

    // solo rank0 ha l’indice globale in `work`
    if (rank == 0) {
        auto& merged = work;

        if (!is_sorted_by_key(merged)) {
            std::cerr << "[error] Global merged index is NOT sorted by key\n";
            MPI_Type_free(&MPI_INDEXREC);
            return 1;
        }
        std::cout << "[ok] Global merged index is sorted by key\n";

        std::string out_path = "data/sorted_mpi_" + std::to_string(p.n_records) + "_" +
                               std::to_string(p.payload_max) + ".bin";

        double tw0 = MPI_Wtime();
        if (!rewrite_sorted_file_mmap(in_path, out_path, merged)) {
            std::cerr << "rewrite_sorted_file_mmap failed\n";
            MPI_Type_free(&MPI_INDEXREC);
            return 1;
        }
        t_write = MPI_Wtime() - tw0;

        double tc0 = MPI_Wtime();
        if (!check_sorted_file_mmap(out_path, p.n_records)) {
            std::cerr << "check_sorted_file_mmap failed\n";
            MPI_Type_free(&MPI_INDEXREC);
            return 1;
        }
        t_check = MPI_Wtime() - tc0;

        std::cout << "[ok] Sorted output file verification OK\n";
    }

    // TIMING per report: uso il MAX (worst rank) per sort/merge/total
    double t_total = MPI_Wtime() - t_total0;

    double t_sort_max = 0.0;
    double t_merge_max = 0.0;
    double t_total_max = 0.0;

    MPI_Reduce(&t_sort,  &t_sort_max,  1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_merge, &t_merge_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_total, &t_total_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    double t_c = t_build + t_sort_max + t_merge_max;
    
    if (rank == 0) {
        std::cout << "algo,n,p,cutoff,threads_per_rank,build_time,sort_time_max,merge_time_max,write_time,check_time,total_time_max, t_c\n";
        std::cout << "mpi,"
                  << p.n_records << ","
                  << p.payload_max << ","
                  << p.cutoff << ","
                  << p.n_threads << ","
                  << t_build << ","
                  << t_sort_max << ","
                  << t_merge_max << ","
                  << t_write << ","
                  << t_check << ","
                  << t_total_max << ","
                  << t_c
                  << "\n";
    }

    MPI_Type_free(&MPI_INDEXREC);
    return 0;
}