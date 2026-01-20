// Driver FastFlow
// -invoca sort_index_ff
// -gestisce numero thread FastFlow
// -stesso schema di misurazione di omp

#include <iostream>
#include "utils.hpp"

//main stub
int main(int argc, char** argv)
{
    Params p = parse_argv(argc, argv);
    std::cout << "FF stub\n";
    std::cout << "records     : " << p.n_records << "\n";
    std::cout << "payload_max : " << p.payload_max << "\n";
    std::cout << "threads     : " << p.n_threads << "\n";
    return 0;
}
