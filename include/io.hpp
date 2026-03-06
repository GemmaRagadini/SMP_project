#ifndef IO_HPP 
#define IO_HPP 

#include "index.hpp"
#include <cstddef>
#include <cstdint>
#include <string> 
#include <vector>
 
using namespace std; 

struct GenStats {
      size_t bytes_written = 0;
}; 

string ensure_unsorted_file(size_t n_records, uint32_t payload_max, GenStats* st= nullptr);

bool rewrite_sorted_file_mmap(const std::string& in_path, const std::string& out_path, const std::vector<IndexRec>& idx);

bool check_sorted_file_mmap(const std::string& path, std::size_t expected_n);


#endif