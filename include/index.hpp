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

#endif