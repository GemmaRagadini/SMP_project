// Eseguibile baseline sequenziale

#include <iostream>
#include <string>
#include "utils.hpp"
#include "io.hpp"
#include "index.hpp"

int main(int argc, char** argv) {

    //legge argomenti passati
    Params p = parse_argv(argc, argv);

    std::cout << "Sequential MergeSort (stub)\n";
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

    return 0;
}
