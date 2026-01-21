// Implementa quello che è dichiarato in io.hpp

#include "io.hpp"

#include <filesystem> 
#include <fstream>
#include <iostream>
#include <random>
#include <vector>


//creazione path
static std::string make_path(std::size_t n, std::uint32_t pmax) {
      return "data/unsorted_"+std::to_string(n)+"_"+std::to_string(pmax)+".bin"; 
}

string ensure_unsorted_file(std::size_t n_records, std::uint32_t payload_max, GenStats* st) {
      namespace fs = std::filesystem;
      fs::create_directories("data"); 

      const std::string path = make_path(n_records, payload_max); 

      if (fs::exists(path)) {
            if (st) st->bytes_written = fs::file_size(path); 
            std::cout << "[io] Reusing existing dataset: " << path << "\n"; 
            return path;
      }

      std::ofstream out(path, std::ios::binary); 
      if (!out){
            throw std::runtime_error("Cannot open output file for writing: " + path);
      }

      // deterministic RNG 
      std::mt19937 rng(42);  
      std::uniform_int_distribution<std::uint64_t> key_dist(0, 0x7fffffffULL);
      std::uniform_int_distribution<std::uint32_t> len_dist(8, payload_max); 
      std::uniform_int_distribution<int> byte_dist(0,255);

      std::vector<char> payload; 
      payload.reserve(payload_max); 

      std::size_t bytes = 0; 

      for (std::size_t i =0 ; i <n_records; ++i){
            const std::uint64_t key = key_dist(rng); 
            const std::uint32_t len = len_dist(rng);  
            payload.resize(len); 
            for (std::uint32_t j = 0; j < len; ++j) {
                  payload[j] = static_cast<char>(byte_dist(rng));
            }

            //write record header + payload 
            out.write(reinterpret_cast<const char*>(&key), sizeof(key)); 
            out.write(reinterpret_cast<const char*>(&len), sizeof(len)); 
            out.write(payload.data(), static_cast<std::streamsize>(len));

            bytes += sizeof(key) + sizeof(len) + len;
      }
      out.close(); 
      if(!out) {
            throw std::runtime_error("Error while writing file: " + path); 
      }

      if (st) st-> bytes_written = bytes;  
      std::cout << "[io] Generated dataset: " <<path << " (" <<bytes << " bytes)\n"; 
      return path;
}