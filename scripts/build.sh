#!/usr/bin/env bash
set -euo pipefail

build_one () {
  local TYPE=$1
  local DIR="build-${TYPE,,}"
  cmake -S . -B "$DIR" -DCMAKE_BUILD_TYPE="$TYPE"
  cmake --build "$DIR" -j
}

if [[ "${1:-}" == "All" ]]; then
  build_one Release
  build_one Debug
else
  build_one "${1:-Release}"
fi