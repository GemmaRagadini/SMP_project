// Driver MPI (multi-node). Qui logica di alto livello:

// MPI_Init / Finalize
// definizione rank/size
// schema di distribuzione dell’indice tra rank
// raccolta e merge globale
// scelta di chi fa I/O (tipicamente rank 0)
// misure di tempo distribuite (MPI_Wtime o chrono + barrier ragionata)

// In un design pulito, main_mpi.cpp chiama funzioni di supporto (vedi sotto) per non diventare enorme.

#include <iostream>
#include "utils.hpp"

//main stub
int main(int argc, char** argv)
{
    Params p = parse_argv(argc, argv);
    std::cout << "MPI stub\n";
    std::cout << "records     : " << p.n_records << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads << "\n";
    return 0;
}
