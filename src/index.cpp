// Implementa:
// scansione del file mmappato
// costruzione dell’array/vector di IndexRec
// aggiornamento progress gate (se presente)

//per adesso lettura sequenziale 

#include "index.hpp"

#include <fstream> 
#include <iostream>
#include <stdexcept> 
#include <algorithm>  // per sort 
#include <cstddef> // per size_t
#include <vector>

std::vector<IndexRec> build_index_streaming(const std::string& path, std::size_t expected_n) {
      std::ifstream in(path, std::ios::binary); 
      if(!in) {
            throw std::runtime_error("Cannot open input file: " + path);
      }

      std::vector<IndexRec> idx; 
      idx.reserve(expected_n); 

      std::uint64_t offset = 0; 
      for (std::size_t i = 0; i < expected_n ;  ++i) {
            std::uint64_t key = 0; 
            std::uint32_t len = 0; 
            //read key 
            in.read(reinterpret_cast<char*>(&key), sizeof(key));
            if (!in) {
                  throw std::runtime_error("Unexpected EOF while reading key at record " + std::to_string(i)); 
            }
            //read len 
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!in) {
                  throw std::runtime_error("Unexpected EOF while reading len at record " + std::to_string(i));
            }           

            idx.push_back(IndexRec{key, offset, len}); 
            //skip payload 
            in.seekg(static_cast<std::streamoff>(len), std::ios::cur); 
            if (!in) throw std::runtime_error("Unexpected EOF while skipping payload at record "+ std::to_string(i));
            offset += sizeof(key) + sizeof(len) + len; 
      }
      std::cout << "[index] Built index for " << idx.size() << " records\n"; 
      return idx;
}


//comparatore unico 
bool index_less(const IndexRec& a, const IndexRec& b) {
      if (a.key != b.key) return a.key < b.key; 
      return a.offset < b.offset; // ulteriore confronto
}

// ordinamento sequenziale per chiave
void sort_index_seq(std::vector<IndexRec>& idx){
      std::sort(idx.begin(), idx.end(), index_less);
}

//controllo ordinamento
bool is_sorted_by_key(const std::vector<IndexRec>& idx){
      for (std::size_t i = 1; i < idx.size(); ++i) {
            if (index_less(idx[i], idx[i-1])) return false; 
      }
      return true;
}

// combina due metà ordinate in un'unica porzione ordinata
static void merge_range(std::vector<IndexRec>&a, std::vector<IndexRec>& tmp, std::size_t left, std::size_t mid, std::size_t right) {
      // mergia a[left:mid) e a[mid:right) in tmp, poi copia in a 
      std::size_t i = left; 
      std::size_t j = mid; 
      std::size_t k = left; 
      while ( i < mid && j < right) {
            if (index_less(a[j], a[i])) tmp[k++] = a[j++]; 
            else tmp[k++]= a[i++];
      }
      while(i < mid) tmp[k++] = a[i++]; 
      while(j < right) tmp[k++] = a[j++];

      if (k != right) {
            std::cerr << "[merge] bug: k=" << k << " right=" << right
                      << " left=" << left << " mid=" << mid << "\n";
            std::abort();
        }if (k != right) {
            std::cerr << "[merge] bug: k=" << k << " right=" << right
                      << " left=" << left << " mid=" << mid << "\n";
            std::abort();
        }
        
      for (std::size_t t = left; t < right; ++t) a[t] = tmp[t];
}

static void mergesort_rec(std::vector<IndexRec>& a, std::vector<IndexRec>& tmp, std::size_t left, std::size_t right) {
      const std::size_t n = right - left; 
      if (n<=1) return; 
      const std::size_t mid = left + n / 2; 
      mergesort_rec(a,tmp, left, mid); 
      mergesort_rec(a, tmp, mid, right); 
      // if(!index_less(a[mid], a[mid -1])) return; // se già in ordine
      merge_range(a,tmp, left, mid, right);
}

void mergesort_index_seq(std::vector<IndexRec>& idx) {
      if (idx.size() <= 1) return; 
      std::vector<IndexRec> tmp(idx.size()); 
      mergesort_rec(idx, tmp, 0, idx.size());
}