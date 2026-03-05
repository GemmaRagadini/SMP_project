// Eseguibile baseline sequenziale / OMP / FastFlow / MPI+OMP

#include <iostream>
#include <string>
#include <chrono>
#include <stdexcept>

#include <omp.h>
#include <mpi.h>

#include "utils.hpp"
#include "io.hpp"
#include "index.hpp"
#include "mpi_merge.hpp"

int main(int argc, char** argv) {

    // legge argomenti passati
    Params p = parse_argv(argc, argv);

    // Branch MPI (multi-nodo)
    if (p.algo == "mpi") {
        int provided = 0;
        MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
        
        int is_main = 0;
        MPI_Is_thread_main(&is_main);
        if (!is_main) {
            std::cerr << "[error] MPI initialized by a non-main thread\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    
        MPI_Comm_size(MPI_COMM_WORLD, &p.np);
        int rank = 0;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
        int rc = run_mpi(p);
    
        MPI_Finalize();
        return rc;
    }

    // -------------------------
    // Single-node (seq/omp/ff)
    // -------------------------
    auto t_total0 = std::chrono::steady_clock::now();

    std::cout << "MergeSort\n";
    std::cout << "algorithm   : " << p.algo << "\n";
    std::cout << "records     : " << p.n_records   << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads   << "\n";

    GenStats st{};
    // si assicura che esista un file binario di input non ordinato e coerente con i parametri
    const std::string in_path = ensure_unsorted_file(p.n_records, p.payload_max, &st);

    // build index
    auto t0 = std::chrono::steady_clock::now();
    auto idx = build_index_mmap(in_path, p.n_records);
    double t_build = seconds_since(t0);

    // sort
    t0 = std::chrono::steady_clock::now();
    if (p.n_threads > 0) {
        omp_set_num_threads((int)p.n_threads);
    }

    if (p.algo == "seq") {
        mergesort_index_seq(idx);
    } else if (p.algo == "omp") {
        mergesort_index_openmp(idx, p.cutoff);
    } else if (p.algo == "ff") {
        mergesort_index_ff(idx, p.cutoff, p.n_threads);
    } else {
        throw std::runtime_error("Unsupported algorithm");
    }
    double t_sort = seconds_since(t0);

    // controllo sorted
    if (!is_sorted_by_key(idx)) {
        std::cerr << "[error] Index is NOT sorted by key\n";
        return 1;
    }
    std::cout << "[ok] Index is sorted by key\n";

    // rewrite + check
    std::string out_path = "data/sorted_" + std::to_string(p.n_records) + "_" +
                           std::to_string(p.payload_max) + ".bin";

    t0 = std::chrono::steady_clock::now();
    if (!rewrite_sorted_file_mmap(in_path, out_path, idx)) {
        std::cerr << "rewrite_sorted_file_mmap failed\n";
        return 1;
    }
    double t_write = seconds_since(t0);

    t0 = std::chrono::steady_clock::now();
    if (!check_sorted_file_mmap(out_path, p.n_records)) {
        std::cerr << "check_sorted_file_mmap failed\n";
        return 1;
    }
    double t_check = seconds_since(t0);

    std::cout << "[ok] Sorted output file verification OK\n";

    double t_total = seconds_since(t_total0);

    // CSV output
    static bool header_printed = false;
    double merge_time = 0.0;
    double kernel_time = t_build + t_sort + merge_time;

    if (!header_printed) {
        std::cout << "algo,n,p,threads,np,build_time,sort_time,merge_time,write_time,check_time,total_time,kernel_time\n";
        header_printed = true;
    }

    int threads = 1;

    #ifdef _OPENMP
    // per OMP: il runtime sa quanti thread userà (dopo omp_set_num_threads)
    if (p.algo == "omp") threads = omp_get_max_threads();
#endif
    // per FastFlow interessa il parametro esplicito
    if (p.algo == "ff" && p.n_threads > 0) threads = (int)p.n_threads;
        
    std::cout
        << p.algo << ","
        << p.n_records << ","
        << p.payload_max << ","
        << threads << ","
        << p.np << ","
        << t_build << ","
        << t_sort << ","
        << merge_time << ","
        << t_write << ","
        << t_check << ","
        << t_total << ","
        << kernel_time
        << "\n";

    return 0;
}