#include "utils.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace std; 

Params parse_argv(int argc, char** argv) {
    
      Params p{};
      auto die = [&](const string& msg) -> void {
            cerr << "Error: " << msg << "\n";
            cerr
                  << "Usage: " << argv[0] << " [options]\n"
                  << "  -r N      number of records (default 1000000)\n"
                  << "  -p B      maximum payload size in bytes, B>=8 (default 256)\n"
                  << "  -t T      threads to use (default 0)\n"
                  << "  -h        show this help\n";
            exit(1);
      };

      for (int i = 1; i < argc; ++i) {
            string a = argv[i];

            auto need_value = [&](const string& opt) -> string {
                  if (i + 1 >= argc) die("Missing value for " + opt);
                  return string(argv[++i]);
            };

            if (a == "-h") {
                  cout
                        << "Usage: " << argv[0] << " [options]\n"
                        << "  -r N      number of records (default 1000000)\n"
                        << "  -p B      maximum payload size in bytes, B>=8 (default 256)\n"
                        << "  -t T      threads to use (default 0)\n"
                        << "  -h        show this help\n";
                  exit(0);
            }
            else if (a == "-n" || a == "-r") {
                  string v = need_value(a);
                  try {
                  p.n_records = static_cast<size_t>(stoull(v));
                  } catch (...) {
                  die("Invalid -r value: " + v);
                  }
                  if (p.n_records == 0) die("-r must be > 0");
            }
            else if (a == "-p") {
                  string v = need_value(a);
                  unsigned long x = 0;
                  try {
                  x = stoul(v);
                  } catch (...) {
                  die("Invalid -p value: " + v);
                  }
                  if (x < 8) die("-p must be >= 8");
                  p.payload_max = static_cast<uint32_t>(x);
            }
            else if (a == "-t") {
                  string v = need_value(a);
                  try {
                  p.n_threads = static_cast<size_t>(stoull(v));
                  } catch (...) {
                  die("Invalid -t value: " + v);
                  }
            }
            else {
                  die("Unknown option: " + a);
            }
      }

    return p;
}
