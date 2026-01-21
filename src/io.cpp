// Implementa quello che è dichiarato in io.hpp

#include "io.hpp"
#include "index.hpp"

#include <filesystem> 
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
#include <stdexcept>
#include <cstdint>

//creazione path
static std::string make_path(std::size_t n, std::uint32_t pmax) {
      return "data/unsorted_"+std::to_string(n)+"_"+std::to_string(pmax)+".bin"; 
}

//genera un file binario non ordinato in data/ e ritorna il suo path. Se già esiste, viene riusato
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

static std::string make_sorted_path_from_unsorted(const std::string& in_path){
      //in path: data/unsorted_N_P.bin => data/sorted_N_P.bin 
      std::string out = in_path; 
      auto pos = out.find("unsorted_"); 
      if (pos != std::string::npos) out.replace(pos, std::string("unsorted_").size(), "sorted_"); 
      else out = "data/sorted.bin"; 
      return out; 
}

// riscrive un file ordinato copiando i record da in_path secondo l'ordine di idx, poi ritorna il path del file 
std::string rewrite_sorted_file_streaming(const std::string& in_path, const std::vector<IndexRec>& idx) {
      const std::string out_path = make_sorted_path_from_unsorted(in_path);
      std::ifstream in(in_path, std::ios::binary);
      if (!in) throw std::runtime_error("Cannot open input file: " + in_path);
      std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
      if (!out) throw std::runtime_error("Cannot open output file: " + out_path);
      std::vector<char> buf;
      for (std::size_t i = 0; i < idx.size(); ++i) {
            const auto& r = idx[i];
            // Dimensione record su disco: key(8) + len(4) + payload(len)
            const std::size_t rec_size = sizeof(std::uint64_t) + sizeof(std::uint32_t) + r.len;
            buf.resize(rec_size);
            // Vai all'offset del record nel file di input
            in.seekg(static_cast<std::streamoff>(r.offset), std::ios::beg);
            if (!in) throw std::runtime_error("seekg failed at record " + std::to_string(i));
            // Leggi l'intero record (header + payload) in buffer
            in.read(buf.data(), static_cast<std::streamsize>(rec_size));
            if (!in) throw std::runtime_error("read failed at record " + std::to_string(i));
            // Scrivi il record nel file di output
            out.write(buf.data(), static_cast<std::streamsize>(rec_size));
            if (!out) throw std::runtime_error("write failed at record " + std::to_string(i));
      }

      out.close();
      in.close();

      std::cout << "[io] Wrote sorted file: " << out_path << "\n";
      return out_path;
}

//verifica che il file sia ordinato per key e contenga expectd_n record 
bool check_sorted_file_streaming(const std::string& path, std::size_t expected_n) {
      std::ifstream in(path, std::ios::binary);
      if (!in) {
            std::cerr << "[check] Cannot open file: " << path << "\n";
            return false;
      }

      std::uint64_t prev_key = 0;
      bool have_prev = false;

      for (std::size_t i = 0; i < expected_n; ++i) {
            std::uint64_t key = 0;
            std::uint32_t len = 0;

            in.read(reinterpret_cast<char*>(&key), sizeof(key));
            if (!in) {
                  std::cerr << "[check] Unexpected EOF while reading key at record " << i << "\n";
                  return false;
            }

            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!in) {
                  std::cerr << "[check] Unexpected EOF while reading len at record " << i << "\n";
                  return false;
            }

            if (have_prev && key < prev_key) {
                  std::cerr << "[check] Out of order at record " << i
                        << ": " << key << " < " << prev_key << "\n";
                  return false;
            }
            prev_key = key;
            have_prev = true;

            // Salta payload
            in.seekg(static_cast<std::streamoff>(len), std::ios::cur);
            if (!in) {
                  std::cerr << "[check] Unexpected EOF while skipping payload at record " << i << "\n";
                  return false;
            }
      }

      // Verifica che non ci siano byte extra oltre expected_n record (controllo “strict”)
      char extra;
      if (in.read(&extra, 1)) {
            std::cerr << "[check] File has extra bytes beyond expected records\n";
            return false;
      }

      std::cout << "[check] File is sorted and has " << expected_n << " records\n";
      return true;
}
