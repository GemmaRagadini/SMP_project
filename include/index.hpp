#ifndef INDEX_HPP   
#define INDEX_HPP 

#include <cstddef>
#include <cstdint>
#include <string> 
#include <vector>

struct IndexRec {
      unsigned long key;   
      uint64_t      offset;
      uint32_t      len;
};

std::vector<IndexRec> build_index_mmap(const std::string& path, std::size_t n);

bool index_less(const IndexRec& a, const IndexRec& b);

void sort_index_seq(std::vector<IndexRec>& idx);

void mergesort_index_openmp(std::vector<IndexRec>& idx, std::size_t cutoff);

void mergesort_index_seq(std::vector<IndexRec>& idx);

bool is_sorted_by_key(const std::vector<IndexRec>& idx);

void mergesort_index_ff(std::vector<IndexRec>& idx, std::size_t cutoff, std::size_t nw);

#endif