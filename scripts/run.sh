#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
shift || true

BUILD_DIR="build-${BUILD_TYPE,,}"
EXEC="$BUILD_DIR/sorter"

if [[ ! -x "$EXEC" ]]; then
  echo "Executable not found: $EXEC"
  echo "Build it first with: ./scripts/build.sh $BUILD_TYPE"
  exit 1
fi

# Defaults (used only if the corresponding flag is NOT provided by the user)
DEFAULT_N=10000
DEFAULT_P=32
DEFAULT_A=seq
DEFAULT_T=4
DEFAULT_C=10000

ARGS=("$@")

has_flag () {
  local flag="$1"
  for a in "${ARGS[@]}"; do
    [[ "$a" == "$flag" ]] && return 0
  done
  return 1
}

# Add defaults only if missing
if ! has_flag "-n"; then ARGS+=("-n" "$DEFAULT_N"); fi
if ! has_flag "-p"; then ARGS+=("-p" "$DEFAULT_P"); fi
if ! has_flag "-a"; then ARGS+=("-a" "$DEFAULT_A"); fi

# Add -t only if missing AND algorithm is not seq (optional, but sensible)
# If you prefer always default -t when missing, remove the algo check.
if ! has_flag "-t"; then
  # detect algo value if present
  algo="$DEFAULT_A"
  for ((i=0; i<${#ARGS[@]}; i++)); do
    if [[ "${ARGS[i]}" == "-a" && $((i+1)) -lt ${#ARGS[@]} ]]; then
      algo="${ARGS[i+1]}"
      break
    fi
  done
  if [[ "$algo" != "seq" ]]; then
    ARGS+=("-t" "$DEFAULT_T")
  fi
fi

# Add cutoff default only if missing AND algorithm is ff (optional)
if ! has_flag "-c"; then
  algo="$DEFAULT_A"
  for ((i=0; i<${#ARGS[@]}; i++)); do
    if [[ "${ARGS[i]}" == "-a" && $((i+1)) -lt ${#ARGS[@]} ]]; then
      algo="${ARGS[i+1]}"
      break
    fi
  done
  if [[ "$algo" == "ff" ]]; then
    ARGS+=("-c" "$DEFAULT_C")
  fi
fi

exec "$EXEC" "${ARGS[@]}"
