// Eseguibile baseline sequenziale

#include <iostream>
#include <string>
#include <omp.h>
#include "utils.hpp"
#include "io.hpp"
#include "index.hpp"

int main(int argc, char** argv) {
    
    auto t_total0 = std::chrono::steady_clock::now();

    //legge argomenti passati
    Params p = parse_argv(argc, argv);

    std::cout << "MergeSort\n";
    std::cout << "algorithm : " << p.algo << "\n";
    std::cout << "records     : " << p.n_records   << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads   << "\n";


    GenStats st{}; 
    //si assicura che esista un file binario di input non ordinato e coerente con i parametri
    const std::string in_path = ensure_unsorted_file(p.n_records, p.payload_max, &st); 

    auto t0 = std::chrono::steady_clock::now(); 
    //legge il file binario e costruisce un indice in RAM contentente solo info per ordinamento 
    auto idx = build_index_streaming(in_path, p.n_records);
    double t_build = seconds_since(t0);

    // //stampa i primi 5 indici 
    // const std::size_t k = std::min<std::size_t>(5, idx.size());
    // for (std::size_t i = 0; i < k; ++i) {
    //     std::cout << "idx[" << i << "]: key=" << idx[i].key
    //             << " offset=" << idx[i].offset
    //             << " len=" << idx[i].len << "\n";
    // }

	//sorting 
    t0 = std::chrono::steady_clock::now();
    if (p.n_threads > 0) {
		omp_set_num_threads(p.n_threads); // per ora 
    }
    if (p.algo == "seq") {
		mergesort_index_seq(idx);
    }
    else if (p.algo == "omp") {
		mergesort_index_openmp(idx, p.cutoff);
    } 
    else if (p.algo == "ff") {
        mergesort_index_ff(idx, p.cutoff, p.n_threads);
    }
    else {
		throw std::runtime_error("Unsupported algorithm");
    }
    double t_sort = seconds_since(t0);


    //controllo 
    if (!is_sorted_by_key(idx)){
        std::cerr << "[error] Index is NOT sorted by key\n";
        return 1;
    }

    std::cout << "[ok] Index is sorted by key\n";
    
    // //stampo di nuovo i primi 5 indici 
    // for (std::size_t i = 0; i < k; ++i) {
    //     std::cout << "idx[" << i << "]: key=" << idx[i].key
    //             << " offset=" << idx[i].offset
    //             << " len=" << idx[i].len << "\n";
    // }

	t0 = std::chrono::steady_clock::now();
    // creazione file output ordinato
    const std::string out_path = rewrite_sorted_file_streaming(in_path, idx);
	double t_write= seconds_since(t0);

	t0 = std::chrono::steady_clock::now();
    if (!check_sorted_file_streaming(out_path, p.n_records)) {
        std::cerr << "[error] Sorted output file verification FAILED\n";
        return 1;
    }
	double t_check = seconds_since(t0);

    std::cout << "[ok] Sorted output file verification OK\n";

    double t_total = seconds_since(t_total0);

	//stampo tempi 
    std::cout << "algo,n,p,cutoff, threads, build_time,sort_time,write_time,check_time,total_time\n";
    // numero thread reale
    int threads = 1;
    #ifdef _OPENMP
    if (p.algo == "omp")
        threads = omp_get_max_threads();
    #endif

    if (p.algo == "ff" && p.n_threads > 0) {
        threads = (int)p.n_threads;
    }

    std::cout
        << p.algo << ","
        << p.n_records << ","
        << p.payload_max << ","
        << p.cutoff << ","
        << threads << ","
        << t_build << ","
        << t_sort << ","
        << t_write << ","
        << t_check << ","
        << t_total
        << "\n";


    return 0;
}


//confronto con oracolo 
#ifdef DEBUG_ORACLE
    auto idx_ref = idx;
    sort_index_seq(idx_ref);
    if (idx_ref.size() != idx.size()) {
        std::cerr << "[error] Oracle size mismatch\n";
        return 1;
    }
    for (std::size_t i = 0; i < idx.size(); ++i) {
        if (idx[i].key != idx_ref[i].key || idx[i].offset != idx_ref[i].offset) {
            std::cerr << "[error] Mismatch vs oracle at i=" << i << "\n";
            return 1;
        }
    }
    std::cout << "[ok] Mergesort matches std::sort oracle\n";
#endif