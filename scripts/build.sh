#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BUILD_TYPE="${BUILD_TYPE:-RelWithDebInfo}"

CXX="${CXX:-}"

EXTRA_CMAKE_ARGS=("$@")

cmake_args=(-S . -B "$BUILD_DIR" "-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
if [[ -n "$CXX" ]]; then
  cmake_args+=("-DCMAKE_CXX_COMPILER=$CXX")
fi

cmake "${cmake_args[@]}" "${EXTRA_CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j
echo "[ok] built in ./$BUILD_DIR"