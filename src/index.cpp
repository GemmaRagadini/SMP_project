// Implementa:
// scansione del file mmappato
// costruzione dell’array/vector di IndexRec
// aggiornamento progress gate (se presente)

//per adesso lettura sequenziale 

#include "index.hpp"

#include <fstream> 
#include <iostream>
#include <stdexcept> 

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