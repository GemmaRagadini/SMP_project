// Eseguibile baseline sequenziale

#include <iostream>
#include <string>
#include <omp.h>
#include "utils.hpp"
#include "io.hpp"
#include "index.hpp"

int main(int argc, char** argv) {

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
    //legge il file binario e costruisce un indice in RAM contentente solo info per ordinamento 
    auto idx = build_index_streaming(in_path, p.n_records);

    //stampa i primi 5 indici 
    const std::size_t k = std::min<std::size_t>(5, idx.size());
    for (std::size_t i = 0; i < k; ++i) {
        std::cout << "idx[" << i << "]: key=" << idx[i].key
                << " offset=" << idx[i].offset
                << " len=" << idx[i].len << "\n";
    }

    //mergesort 
    if (p.n_threads > 0) {
      omp_set_num_threads(p.n_threads); // per ora 
    }
    
    if (p.algo == "seq") {
      mergesort_index_seq(idx);
    }
    else if (p.algo == "omp") {
      mergesort_index_openmp(idx, 10000);
    }
    else {
      throw std::runtime_error("Unsupported algorithm");
    }

    //controllo 
    if (!is_sorted_by_key(idx)){
        std::cerr << "[error] Index is NOT sorted by key\n";
        return 1;
    }

    std::cout << "[ok] Index is sorted by key\n";
    
    //stampo di nuovo i primi 5 indici 
    for (std::size_t i = 0; i < k; ++i) {
        std::cout << "idx[" << i << "]: key=" << idx[i].key
                << " offset=" << idx[i].offset
                << " len=" << idx[i].len << "\n";
    }
    
    // creazione file output ordinato
    const std::string out_path = rewrite_sorted_file_streaming(in_path, idx);

    if (!check_sorted_file_streaming(out_path, p.n_records)) {
        std::cerr << "[error] Sorted output file verification FAILED\n";
        return 1;
    }

    std::cout << "[ok] Sorted output file verification OK\n";

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