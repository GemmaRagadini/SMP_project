#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j