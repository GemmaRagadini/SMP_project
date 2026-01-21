// costruire e gestire l’indice in RAM.

#ifndef INDEX_HPP   
#define INDEX_HPP 

#include <cstddef>
#include <cstdint>
#include <string> 
#include <vector>

struct IndexRec {
      std::uint64_t key; 
      std::uint64_t offset; //offstet nel file dove inizia il record 
      std::uint32_t len; 
}; 

std::vector<IndexRec> build_index_streaming(const std::string& path, std::size_t expected_n);  

void sort_index_seq(std::vector<IndexRec>& idx);

// Verifica che idx sia non-decrescente per key (e coerente in caso di key uguali)
bool is_sorted_by_key(const std::vector<IndexRec>& idx);

#endif