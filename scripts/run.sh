#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
APPEND_CSV=""

ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --builddir)     BUILD_DIR="$2"; shift 2;;
    --append-csv)   APPEND_CSV="$2"; shift 2;;
    *)              ARGS+=("$1"); shift;;
  esac
done

EXEC="$BUILD_DIR/sorter"

if [[ ! -x "$EXEC" ]]; then
  echo "Executable not found: $EXEC" >&2
  echo "Build it first with: ./build.sh" >&2
  exit 1
fi

# Default parametri
DEFAULT_N=100000
DEFAULT_P=256
DEFAULT_A=seq
DEFAULT_T=1

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

# Detect algo
algo="$DEFAULT_A"
for ((i=0; i<${#ARGS[@]}; i++)); do
  if [[ "${ARGS[i]}" == "-a" && $((i+1)) -lt ${#ARGS[@]} ]]; then
    algo="${ARGS[i+1]}"
    break
  fi
done

if ! has_flag "-t" && [[ "$algo" != "seq" ]]; then
  ARGS+=("-t" "$DEFAULT_T")
fi

append_csv() {
  local tmp="$1"
  local dst="$2"

  awk -F',' '
    /^algo[[:space:]]*,/ {print; next}
    /^[a-zA-Z_]+[[:space:]]*,[[:space:]]*[0-9]+/ {print}
  ' "$tmp" > "$tmp.filtered"

  if [[ ! -s "$tmp.filtered" ]]; then
    echo "[warn] No CSV lines detected in program output (see raw output below):" >&2
    cat "$tmp" >&2
    rm -f "$tmp.filtered"
    return 0
  fi

  if [[ ! -s "$dst" ]]; then
    cat "$tmp.filtered" >> "$dst"
  else
    awk -F',' '$1!="algo"{print}' "$tmp.filtered" >> "$dst"
  fi

  rm -f "$tmp.filtered"
}

if [[ "$algo" == "mpi" ]]; then
  echo "[error] algo=mpi not supported in run.sh anymore. Use the strong/weak scripts with srun." >&2
  exit 2
fi

if [[ -z "$APPEND_CSV" ]]; then
  "$EXEC" "${ARGS[@]}"
  exit 0
else
  tmp="$(mktemp)"
  ( "$EXEC" "${ARGS[@]}" ) &> "$tmp" || {
    echo "[run.sh] sorter failed (algo=$algo). Raw output saved in: $tmp" >&2
    cat "$tmp" >&2
    exit 1
  }
  append_csv "$tmp" "$APPEND_CSV"
  rm -f "$tmp"
  echo "[ok] appended CSV -> $APPEND_CSV"
fi