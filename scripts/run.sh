#!/usr/bin/env bash

set -euo pipefail

./build/sorter -n 5000000 -p 32 -a omp -t 8