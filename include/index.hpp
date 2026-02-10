// costruire e gestire l’indice in RAM.

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

//costruisci indice
std::vector<IndexRec> build_index_mmap(const std::string& path, std::size_t n);

//comparatore unico 
bool index_less(const IndexRec& a, const IndexRec& b);

//oracolo 
void sort_index_seq(std::vector<IndexRec>& idx);

void mergesort_index_openmp(std::vector<IndexRec>& idx, std::size_t cutoff);

//mergesort sequenziale mio  
void mergesort_index_seq(std::vector<IndexRec>& idx);

//check
bool is_sorted_by_key(const std::vector<IndexRec>& idx);

void mergesort_index_ff(std::vector<IndexRec>& idx, std::size_t cutoff, std::size_t nw);

#endif