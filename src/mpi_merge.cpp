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

// Crea un MPI_Datatype che descrive IndexRec 
static MPI_Datatype make_mpi_indexrec_type() {
	
	MPI_Datatype t;
	IndexRec dummy{};
	
	// indirizzo base
	MPI_Aint base;
	MPI_Get_address(&dummy, &base);

	// displacement dei 3 campi relativamente al base
	MPI_Aint displs[3];
	MPI_Get_address(&dummy.key, &displs[0]);
	MPI_Get_address(&dummy.offset, &displs[1]);
	MPI_Get_address(&dummy.len, &displs[2]);
	for (int i = 0; i < 3; ++i) displs[i] -= base;

	// un elemento per ogni campo
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


// merge a k vie di P segmenti ordinati che sono dentro all_sorted_segments
// counts[s] = lunghezza del segmento s
// displs[s] = offset di inizio s dentro all_sorted_segments
static std::vector<IndexRec> kway_merge(const std::vector<IndexRec>& all_sorted_segments, const std::vector<int>& counts, const std::vector<int>& displs) {
    
	const int P = (int)counts.size();
    struct Node {
        IndexRec rec;      // candidato
        int src;           // in quale segmento
        int pos_in_src;    // in che posizione
    };

    // creazione min-heap. In pq.top() c'è il record con key minima
    auto cmp = [](const Node& a, const Node& b) { // quale dei due ha priorità più alta
        return index_less(b.rec, a.rec);
    };
    std::priority_queue<Node, std::vector<Node>, decltype(cmp)> pq(cmp);

    // inizializzazione con il primo el di ogni segmento non vuoto
    for (int s = 0; s < P; ++s) {
        if (counts[s] > 0) {
            const IndexRec& r0 = all_sorted_segments[(std::size_t)displs[s]];
            pq.push(Node{r0, s, 0});
        }
    }

	//preallocazione per output
    std::vector<IndexRec> out;
    out.reserve(all_sorted_segments.size()); 

    while (!pq.empty()) {
		
		//estraggo minimo e aggiungo all'output
        Node n = pq.top(); 
		pq.pop(); 
        out.push_back(n.rec);

        const int s = n.src;
        const int next_pos = n.pos_in_src + 1; 
		// next_pos nuovo candidato se non supera la lunghezza del segmento
        if (next_pos < counts[s]) { 
            const IndexRec& rn =
                all_sorted_segments[(std::size_t)displs[s] + (std::size_t)next_pos];
            pq.push(Node{rn, s, next_pos});
        }
    }
    return out;
}


int run_mpi(const Params& p) {
    
	//setup
	int rank = 0, P = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    MPI_Datatype MPI_INDEXREC = make_mpi_indexrec_type();

    // timer
    double t_total0 = MPI_Wtime();
    double t_build = 0.0; //costruzione indice
    double t_sort  = 0.0; //sorting 
    double t_part  = 0.0;  // sampling + splitter + bucketization + alltoallv
    double t_merge = 0.0;  // merging

    std::string in_path;
    std::vector<IndexRec> global_idx;

    // rank0 costuisce dataset + indice globale
    if (rank == 0) {
        GenStats st{};
        in_path = ensure_unsorted_file(p.n_records, p.payload_max, &st);

        double t0 = MPI_Wtime();
        global_idx = build_index_mmap(in_path, p.n_records);
        t_build = MPI_Wtime() - t0;
    }

    // broadcast del path del file
    int path_len = 0;
    if (rank == 0) path_len = (int)in_path.size();
    MPI_Bcast(&path_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) in_path.resize((std::size_t)path_len);
    MPI_Bcast(in_path.data(), path_len, MPI_CHAR, 0, MPI_COMM_WORLD);

    // rank0 compute counts/displs 
    std::vector<int> counts, displs;
    if (rank == 0) {
        counts.assign(P, 0);
        displs.assign(P, 0);
		
		// si divide N record in P blocchi contigui
        const std::size_t N = p.n_records;
        for (int r = 0; r < P; ++r) {
            std::size_t start = (std::uint64_t(r) * N) / (std::uint64_t)P;
            std::size_t end   = (std::uint64_t(r + 1) * N) / (std::uint64_t)P;
            counts[r] = (int)(end - start);
        }
        for (int r = 1; r < P; ++r) displs[r] = displs[r - 1] + counts[r - 1];

		//check
        if ((std::size_t)(displs.back() + counts.back()) != N) {
            MPI_Type_free(&MPI_INDEXREC);
            throw std::runtime_error("Internal error: counts/displs do not sum to N");
        }
    }

    // Scatter della dimensione locale 
    int recvcount = 0; //dim locale
    MPI_Scatter(rank == 0 ? counts.data() : nullptr, 1, MPI_INT, &recvcount, 1, MPI_INT, 0, MPI_COMM_WORLD);
 
	std::vector<IndexRec> local_idx((std::size_t)recvcount);

    // Scatterv dei dati - distribuzione indice per posizione - ogni rank riceve il proprio local_idx 
    MPI_Scatterv(rank == 0 ? global_idx.data() : nullptr, rank == 0 ? counts.data() : nullptr, rank == 0 ? displs.data() : nullptr, MPI_INDEXREC, local_idx.data(), recvcount, MPI_INDEXREC, 0, MPI_COMM_WORLD);

    //  sort locale con OMP
    MPI_Barrier(MPI_COMM_WORLD);
    double t0s = MPI_Wtime();

    if (p.n_threads > 0) omp_set_num_threads((int)p.n_threads);
    mergesort_index_openmp(local_idx, p.cutoff);

    t_sort = MPI_Wtime() - t0s;

    // sanity: local sorted
    int ok_local = is_sorted_by_key(local_idx) ? 1 : 0;
    int ok_all = 0;
    MPI_Allreduce(&ok_local, &ok_all, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!ok_all) {
        if (rank == 0) std::cerr << "[error] At least one rank produced a non-sorted local chunk\n";
        MPI_Type_free(&MPI_INDEXREC);
        return 1;
    }

	//Sampling per scegliere gli splitters  

    MPI_Barrier(MPI_COMM_WORLD);
    double t0p = MPI_Wtime();

	//estrazione campione locale equispaziato
    using KeyT = unsigned long;

    const int S = 1024; 
    const int local_n = (int)local_idx.size();
    const int s_local = std::min(S, local_n);

    std::vector<KeyT> sample_local;
    sample_local.reserve((std::size_t)s_local);

    if (s_local > 0) {
        for (int i = 0; i < s_local; ++i) {
            int pos = (int)(((long long)i * (long long)local_n) / (long long)s_local);
            if (pos >= local_n) pos = local_n - 1;
            sample_local.push_back((KeyT)local_idx[(std::size_t)pos].key);
        }
    }

    int s_count = (int)sample_local.size();

    std::vector<int> s_counts, s_displs;
    std::vector<KeyT> sample_all;

    if (rank == 0) s_counts.resize(P);

	//Gather dimensioni su rank 0 per sapere quanti chiavi arrivano da ciascun rank
    MPI_Gather(&s_count, 1, MPI_INT, rank == 0 ? s_counts.data() : nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int s_total = 0;
    if (rank == 0) {
        s_displs.resize(P);
        s_displs[0] = 0;
        for (int r = 0; r < P; ++r) s_total += s_counts[r];
        for (int r = 1; r < P; ++r) s_displs[r] = s_displs[r - 1] + s_counts[r - 1];
        sample_all.resize((std::size_t)s_total);
    }

	//Gaherv su rank raccoglie tutto in sample_all
    MPI_Gatherv(sample_local.data(), s_count, MPI_UNSIGNED_LONG, rank == 0 ? sample_all.data() : nullptr, rank == 0 ? s_counts.data() : nullptr, rank == 0 ? s_displs.data() : nullptr, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

    // rank 0 ordina i campioni e sceglie P-1 splitters ai quantili
    std::vector<KeyT> splitters((std::size_t)std::max(0, P - 1));

    if (rank == 0) {
        if (!sample_all.empty()) {
            std::sort(sample_all.begin(), sample_all.end());
            for (int k = 1; k < P; ++k) {
                long long idx = (long long)k * (long long)sample_all.size() / (long long)P;
                if (idx >= (long long)sample_all.size()) idx = (long long)sample_all.size() - 1;
                splitters[(std::size_t)(k - 1)] = sample_all[(std::size_t)idx];
            }
        } else {
            // degenerate: no samples -> all splitters = 0
            for (int k = 0; k < P - 1; ++k) splitters[(std::size_t)k] = 0;
        }
    }

	//broadcast splitters
    if (!splitters.empty()) {
        MPI_Bcast(splitters.data(), (int)splitters.size(), MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    }


	//bucketizzazione locale (local_idx ordinato)
	std::vector<int> sendcounts(P, 0), sdispls2(P, 0); //sendcounts[r] = cuts[r+1]-cuts[r] -> quanti el mandare al rank r

    // cuts[r] = starting index of bucket r in local_idx 
    std::vector<int> cuts((std::size_t)(P + 1), 0);
    cuts[0] = 0;
    cuts[P] = local_n;

    auto key_less = [](const IndexRec& rec, const KeyT k) {
        return (KeyT)rec.key < k;
    };

    int prev = 0;
    for (int b = 0; b < P - 1; ++b) {
        const KeyT split = splitters[(std::size_t)b];
        auto it = std::lower_bound(local_idx.begin() + prev, local_idx.end(), split, key_less);
        int pos = (int)std::distance(local_idx.begin(), it);
        cuts[b + 1] = pos;
        prev = pos;
    }

    for (int r = 0; r < P; ++r) {
        int a = cuts[r];
        int b = cuts[r + 1];
        sendcounts[r] = b - a;
    }

    sdispls2[0] = 0; //sdispls2 sono i displacement locali per Alltoallv 
    for (int r = 1; r < P; ++r) sdispls2[r] = sdispls2[r - 1] + sendcounts[r - 1];

    std::vector<int> recvcounts(P, 0), rdispls(P, 0);

	// scambio conteggi - ogni rank sa quanti elementi riceverà da ciascun altro
    MPI_Alltoall(sendcounts.data(), 1, MPI_INT, recvcounts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    int recv_total = 0;
    for (int r = 0; r < P; ++r) recv_total += recvcounts[r];

	//prefix sum di recvcounts
    rdispls[0] = 0;
    for (int r = 1; r < P; ++r) rdispls[r] = rdispls[r - 1] + recvcounts[r - 1];

    std::vector<IndexRec> recvbuf((std::size_t)recv_total);

	//scambio bucket - rank r riceve record con chiavi nel suo range globale - record concatenazione di P segmenti , ognuno già ordinato
    MPI_Alltoallv(local_idx.data(), sendcounts.data(), sdispls2.data(), MPI_INDEXREC, recvbuf.data(), recvcounts.data(), rdispls.data(), MPI_INDEXREC, MPI_COMM_WORLD);

    t_part = MPI_Wtime() - t0p; //tempo di sampling + splitter + bucketization + alltoallv

    // merge locale finale
    MPI_Barrier(MPI_COMM_WORLD);
    double t0m = MPI_Wtime();

    std::vector<IndexRec> local_final = kway_merge(recvbuf, recvcounts, rdispls);

    t_merge = MPI_Wtime() - t0m;

	//check locale
	int ok_local2 = is_sorted_by_key(local_final) ? 1 : 0;
    int ok_all2 = 0;
    MPI_Allreduce(&ok_local2, &ok_all2, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!ok_all2) {
        if (rank == 0) std::cerr << "[error] At least one rank produced a non-sorted final segment\n";
        MPI_Type_free(&MPI_INDEXREC);
        return 1;
    }

    KeyT local_min = 0, local_max = 0;
    int has_any = local_final.empty() ? 0 : 1;
    if (has_any) {
        local_min = (KeyT)local_final.front().key;
        local_max = (KeyT)local_final.back().key;
    }

    std::vector<KeyT> mins, maxs;
    std::vector<int> hass;
    if (rank == 0) {
        mins.resize((std::size_t)P);
        maxs.resize((std::size_t)P);
        hass.resize((std::size_t)P);
    }

	//raccolta su rank 0 
    MPI_Gather(&has_any, 1, MPI_INT, rank == 0 ? hass.data() : nullptr, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_min, 1, MPI_UNSIGNED_LONG, rank == 0 ? mins.data() : nullptr, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    MPI_Gather(&local_max, 1, MPI_UNSIGNED_LONG, rank == 0 ? maxs.data() : nullptr, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

	//check
    if (rank == 0) {
        bool ok_bounds = true;
        for (int r = 0; r < P - 1; ++r) {
            if (hass[(std::size_t)r] && hass[(std::size_t)(r + 1)]) {
                if (maxs[(std::size_t)r] > mins[(std::size_t)(r + 1)]) {
                    ok_bounds = false;
                    std::cerr << "[error] Global boundary violated between rank " << r
                              << " and rank " << (r + 1) << " : "
                              << maxs[(std::size_t)r] << " > " << mins[(std::size_t)(r + 1)] << "\n";
                    break;
                }
            }
        }
        if (ok_bounds) {
            std::cout << "[ok] Global boundary check OK (rank ranges are ordered)\n";
        }
    }

	// prendo tempi massimi 
    double t_total = MPI_Wtime() - t_total0;

    double sort_max = 0.0, part_max = 0.0, merge_max = 0.0, total_max = 0.0;
    MPI_Reduce(&t_sort,  &sort_max,  1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_part,  &part_max,  1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_merge, &merge_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_total, &total_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    double kernel_time = t_build + sort_max + part_max + merge_max;

    if (rank == 0) {
        std::cout << "algo,n,p,threads,np,build_time,sort_time,part_time,merge_time,total_time,kernel_time\n";
        std::cout << "mpi_part,"
                  << p.n_records << ","
                  << p.payload_max << ","
                  << p.n_threads << ","
                  << p.np << ","
                  << t_build << ","
                  << sort_max << ","
                  << part_max << ","
                  << merge_max << ","
                  << total_max << ","
                  << kernel_time
                  << "\n";
    }

    MPI_Type_free(&MPI_INDEXREC);
    return 0;
}