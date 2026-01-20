// Driver OpenMP con
// build index (forse 1 thread)
// sorting con sort_index_omp
// rewrite + validate
// Qui configurazione di: 
// -numero thread OpenMP
// - cutoff task size
// -metriche: tempo build+sort (e separatamente rewrite se vuoi)

#include <iostream>
#include "utils.hpp"

//main stub
int main(int argc, char** argv)
{
    Params p = parse_argv(argc, argv);
    std::cout << "OMP stub\n";
    std::cout << "records     : " << p.n_records << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads << "\n";
    return 0;
}
