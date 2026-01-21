// I/O e layout record. 

#ifndef IO_HPP 
#define IO_HPP 

#include "index.hpp"
#include <cstddef>
#include <cstdint>
#include <string> 
#include <vector>
 
using namespace std; 

// formato file (sul disco): [key:u64][len:u32][payload:len bytes] repeated N times 

struct GenStats {
      size_t bytes_written = 0;
}; 

//genera un file binario non ordinato in data/ e ritorna il suo path. Se già esiste, viene riusato
string ensure_unsorted_file(size_t n_records, uint32_t payload_max, GenStats* st= nullptr);


// riscrive un file ordinato copiando i record da in_path secondo l'ordine di idx, poi ritorna il path del file 
std::string rewrite_sorted_file_streaming(const std::string& in_path, const std::vector<struct IndexRec>& idx); 

//verifica che il file sia ordinato per key e contenga expectd_n record 
bool check_sorted_file_streaming(const std::string& path, std::size_t expected_n);

#endif