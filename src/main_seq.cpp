// Eseguibile baseline sequenziale

#include <iostream>
#include <string>
#include "utils.hpp"

int main(int argc, char** argv)
{
    Params p = parse_argv(argc, argv);

    std::cout << "Sequential MergeSort (stub)\n";
    std::cout << "records     : " << p.n_records   << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads   << "\n";

    return 0;
}
