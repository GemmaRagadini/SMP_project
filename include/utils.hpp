// struct Params + parsing argomenti (CLI)
// macro o funzioni di timing (BENCH_START/STOP o classe Timer)
// funzioni helper piccole (es. die(perror), align, human_readable_size)
// tipi, costanti, comparatori

#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef>
#include <cstdint>
#include <string>

// Parametri minimi
struct Params {
    std::size_t   n_records   = 1'000'000;
    std::uint32_t payload_max = 256;
    std::size_t   n_threads   = 0;
};

Params parse_argv(int argc, char** argv);

#endif
