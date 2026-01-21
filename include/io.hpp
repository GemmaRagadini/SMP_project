// I/O e layout record. 

#ifndef IO_HPP 
#define IO_HPP 

#include <cstddef>
#include <cstdint>
#include <string> 
 
using namespace std; 

// formato file (sul disco): [key:u64][len:u32][payload:len bytes] repeated N times 

struct GenStats {
      size_t bytes_written = 0;
}; 

//genera un file binario non ordinato in data/ e ritorna il suo path. Se già esiste, viene riusato
string ensure_unsorted_file(size_t n_records, uint32_t payload_max, GenStats* st= nullptr);

#endif