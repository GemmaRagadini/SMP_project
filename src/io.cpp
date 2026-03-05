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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>   // memcpy


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
	std::uniform_int_distribution<unsigned long> key_dist(0, 0x7fffffffULL);
	std::uniform_int_distribution<std::uint32_t> len_dist(8, payload_max); 
	std::uniform_int_distribution<int> byte_dist(0,255);

	std::vector<char> payload; 
	payload.reserve(payload_max); 

	std::size_t bytes = 0; 

	for (std::size_t i =0 ; i <n_records; ++i){
		const unsigned long key = key_dist(rng); 
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

bool rewrite_sorted_file_mmap(const std::string& in_path, const std::string& out_path, const std::vector<IndexRec>& idx) {
      
	// 1) open+stat input
	int fd_in = ::open(in_path.c_str(), O_RDONLY);
	if (fd_in < 0) { perror("open in"); return false; }

	struct stat st{};
	if (fstat(fd_in, &st) < 0) { perror("fstat in"); ::close(fd_in); return false; }
	const std::size_t in_sz = static_cast<std::size_t>(st.st_size);

	// 2) mmap input RO
	const char* in_map = static_cast<const char*>(
	::mmap(nullptr, in_sz, PROT_READ, MAP_SHARED, fd_in, 0)
	);
	if (in_map == MAP_FAILED) { perror("mmap in"); ::close(fd_in); return false; }

	// 3) compute out size (sum of record sizes)
	std::size_t out_sz = 0;
	for (const auto& r : idx) {
	out_sz += sizeof(r.key) + sizeof(r.len) + r.len;
	}

	// 4) open+truncate output
	int fd_out = ::open(out_path.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);
	if (fd_out < 0) {
	perror("open out");
	::munmap((void*)in_map, in_sz);
	::close(fd_in);
	return false;
	}
	if (ftruncate(fd_out, static_cast<off_t>(out_sz)) < 0) {
	perror("ftruncate out");
	::close(fd_out);
	::munmap((void*)in_map, in_sz);
	::close(fd_in);
	return false;
	}

	// 5) mmap output WO
	char* out_map = static_cast<char*>(
	::mmap(nullptr, out_sz, PROT_WRITE, MAP_SHARED, fd_out, 0)
	);
	if (out_map == MAP_FAILED) {
	perror("mmap out");
	::close(fd_out);
	::munmap((void*)in_map, in_sz);
	::close(fd_in);
	return false;
	}

	// 6) copy records in sorted order
	std::size_t out_off = 0;
	for (std::size_t i = 0; i < idx.size(); ++i) {
	const IndexRec& r = idx[i];
	const std::size_t rec_sz = sizeof(r.key) + sizeof(r.len) + r.len;

	// bounds safety on input
	if (r.offset + rec_sz > in_sz) {
	std::fprintf(stderr, "[rewrite] bad offset/size at i=%zu (off=%llu rec_sz=%zu in_sz=%zu)\n",
	i, (unsigned long long)r.offset, rec_sz, in_sz);
	::munmap(out_map, out_sz);
	::munmap((void*)in_map, in_sz);
	::close(fd_out);
	::close(fd_in);
	return false;
	}

	std::memcpy(out_map + out_off, in_map + r.offset, rec_sz);
	out_off += rec_sz;
	}

	// 7) cleanup
	::munmap(out_map, out_sz);
	::munmap((void*)in_map, in_sz);
	::close(fd_out);
	::close(fd_in);

	return true;
}

bool check_sorted_file_mmap(const std::string& path, std::size_t expected_n) {
     
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) { perror("open"); return false; }

	struct stat st{};
	if (fstat(fd, &st) < 0) { perror("fstat"); ::close(fd); return false; }
	const std::size_t sz = static_cast<std::size_t>(st.st_size);

	const unsigned char* base = static_cast<const unsigned char*>(
		::mmap(nullptr, sz, PROT_READ, MAP_SHARED, fd, 0)
	);
	if (base == MAP_FAILED) { perror("mmap"); ::close(fd); return false; }

	std::size_t pos = 0;
	unsigned long prev_key = 0;
	bool have_prev = false;

	for (std::size_t i = 0; i < expected_n; ++i) {
		unsigned long key;
		std::uint32_t len;

		if (pos + sizeof(key) + sizeof(len) > sz) {
				std::cerr << "[check] Unexpected EOF in header at record " << i << "\n";
				::munmap((void*)base, sz);
				::close(fd);
				return false;
		}

		std::memcpy(&key, base + pos, sizeof(key));
		pos += sizeof(key);

		std::memcpy(&len, base + pos, sizeof(len));
		pos += sizeof(len);

		if (pos + len > sz) {
				std::cerr << "[check] Unexpected EOF in payload at record " << i << "\n";
				::munmap((void*)base, sz);
				::close(fd);
				return false;
		}

		if (have_prev && key < prev_key) {
				std::cerr << "[check] Out of order at record " << i
					<< ": " << key << " < " << prev_key << "\n";
				::munmap((void*)base, sz);
				::close(fd);
				return false;
		}
		prev_key = key;
		have_prev = true;

		pos += len;
	}

	// strict: after expected_n records, must be exactly EOF
	if (pos != sz) {
		std::cerr << "[check] File has extra bytes (pos=" << pos << " sz=" << sz << ")\n";
		::munmap((void*)base, sz);
		::close(fd);
		return false;
	}

	::munmap((void*)base, sz);
	::close(fd);
	std::cout << "[check] File is sorted and has " << expected_n << " records\n";
	return true;
}