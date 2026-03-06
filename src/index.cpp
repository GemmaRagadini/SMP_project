#include "index.hpp"
#include <fstream> 
#include <iostream>
#include <stdexcept> 
#include <algorithm>  
#include <cstddef> 
#include <vector>
#include <omp.h>
#include <ff/ff.hpp>          
#include <ff/parallel_for.hpp> 
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Builds the in-memory index of the dataset using memory mapping
std::vector<IndexRec> build_index_mmap(const std::string& path, std::size_t n) {
	int fd = ::open(path.c_str(), O_RDONLY);
	if (fd < 0) throw std::runtime_error("open failed: " + path);

	struct stat st{};
	if (fstat(fd, &st) < 0) {
		::close(fd);
		throw std::runtime_error("fstat failed: " + path);
	}

	const std::size_t file_sz = static_cast<std::size_t>(st.st_size);

	const unsigned char* base = static_cast<const unsigned char*>(
		::mmap(nullptr, file_sz, PROT_READ, MAP_SHARED, fd, 0)
	);
	if (base == MAP_FAILED) {
		::close(fd);
		throw std::runtime_error("mmap failed: " + path);
	}

	std::vector<IndexRec> idx(n);

	std::size_t pos = 0;
	for (std::size_t i = 0; i < n; ++i) {
		const std::size_t rec_off = pos;

		unsigned long key;
		std::uint32_t len;

		if (pos + sizeof(key) + sizeof(len) > file_sz) {
			::munmap((void*)base, file_sz);
			::close(fd);
			throw std::runtime_error("EOF in header at record " + std::to_string(i));
		}

		std::memcpy(&key, base + pos, sizeof(key));
		pos += sizeof(key);

		std::memcpy(&len, base + pos, sizeof(len));
		pos += sizeof(len);

		if (pos + len > file_sz) {
			::munmap((void*)base, file_sz);
			::close(fd);
			throw std::runtime_error("EOF in payload at record " + std::to_string(i));
		}

		idx[i] = IndexRec{ key, static_cast<std::uint64_t>(rec_off), len };
		pos += len;
	}

	::munmap((void*)base, file_sz);
	::close(fd);
	return idx;
}


//comparison
bool index_less(const IndexRec& a, const IndexRec& b) {
	if (a.key != b.key) return a.key < b.key; 
	return a.offset < b.offset; 
}

//sorting check
bool is_sorted_by_key(const std::vector<IndexRec>& idx){
	for (std::size_t i = 1; i < idx.size(); ++i) {
		if (index_less(idx[i], idx[i-1])) return false; 
	}
	return true;
}

// Merges two adjacent sorted ranges [left, mid) and [mid, right) of the index array into a temporary buffer.
static void merge_range(std::vector<IndexRec>&a, std::vector<IndexRec>& tmp, std::size_t left, std::size_t mid, std::size_t right) {
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
	}
	
	for (std::size_t t = left; t < right; ++t) a[t] = tmp[t];
}


//SEQUENTIAL VERSION	
static void mergesort_rec(std::vector<IndexRec>& a, std::vector<IndexRec>& tmp, std::size_t left, std::size_t right) {
	const std::size_t n = right - left; 
	if (n<=1) return; 
	const std::size_t mid = left + n / 2; 
	mergesort_rec(a,tmp, left, mid); 
	mergesort_rec(a, tmp, mid, right); 
	merge_range(a,tmp, left, mid, right);
}

void mergesort_index_seq(std::vector<IndexRec>& idx) {
	if (idx.size() <= 1) return; 
	std::vector<IndexRec> tmp(idx.size()); 
	mergesort_rec(idx, tmp, 0, idx.size());
}



// OMP VERSION
static void mergesort_rec_parallel(std::vector<IndexRec>& a, std::vector<IndexRec>& tmp, std::size_t left, std::size_t right, std::size_t cutoff){
	const std::size_t n = right-left; 
	if (n <= cutoff) {
		std::sort(a.begin()+left, a.begin()+right, index_less); 
		return;
	}
	const std::size_t mid = left +n / 2;
	#pragma omp task shared(a,tmp)
	mergesort_rec_parallel(a,tmp,left, mid,cutoff);
	#pragma omp task shared(a,tmp) 
	mergesort_rec_parallel(a,tmp,mid, right, cutoff);
	#pragma omp taskwait 
	merge_range(a,tmp, left, mid , right);
} 

void mergesort_index_openmp(std::vector<IndexRec>& idx, std::size_t cutoff) {
	if (idx.size() <= 1) return; 
	std::vector<IndexRec> tmp(idx.size());
	#pragma omp parallel 
	{
		#pragma omp single
		mergesort_rec_parallel(idx, tmp, 0, idx.size(), cutoff);
	}
}


// FASTFLOW VERSION
void mergesort_index_ff(std::vector<IndexRec>& idx, std::size_t cutoff, std::size_t nw) {
      
	const std::size_t n = idx.size(); 
	if (n<=1) return; 

	std::size_t block = std::max<std::size_t>(1, cutoff); 
	if (block > n) block = n;  

	std::size_t workers = (nw > 0) ? nw : (std::size_t) std::max<ssize_t>(1, ff_numCores()); 
	if (workers == 0) workers = 1;


	ff::ParallelFor pf((int)workers); 
	pf.parallel_for(std::size_t(0), n, block, [&](const std::size_t l) {
		const std::size_t r = std::min(l+block, n);
		std::sort(idx.begin()+l, idx.begin() +r, index_less);
	});

	std::vector<IndexRec> tmp(n); 
	auto *src = &idx; 
	auto *dst = &tmp;

	for (std::size_t width = block; width < n; width<<=1) {
		const std::size_t step = width << 1; 
		pf.parallel_for(std::size_t(0), n, step, [&](const std::size_t left) {
				const std::size_t mid = std::min(left + width, n); 
				const std::size_t right = std::min(left + step, n);

				std::size_t i = left, j = mid, k = left; 
				while( i < mid && j < right) {
					if (index_less((*src)[j], (*src)[i]))(*dst)[k++] = (*src)[j++];
					else (*dst)[k++]=(*src)[i++];
				}
				while (i < mid) {
					(*dst)[k++] = (*src)[i++];
				}
				while (j < right) {
					(*dst)[k++] = (*src)[j++];
				}
		});
		std::swap(src, dst);
	}
	if (src !=&idx) idx = std::move(*src);
}
