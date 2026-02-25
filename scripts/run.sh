#!/usr/bin/env bash
set -euo pipefail

# Uso:
#   ./run.sh [--builddir build] [-a seq|omp|ff|mpi ...]
#   ./run.sh --append-csv results.csv -a omp -n 100000 -p 256 -t 8
#   ./run.sh --np 8 -a mpi -n 1000000 -p 256 -t 4
#
# Build dir default: build
BUILD_DIR="build"
APPEND_CSV=""

ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --builddir) BUILD_DIR="$2"; shift 2;;
    --append-csv) APPEND_CSV="$2"; shift 2;;
    *) ARGS+=("$1"); shift;;
  esac
done

EXEC="$BUILD_DIR/sorter"

if [[ ! -x "$EXEC" ]]; then
  echo "Executable not found: $EXEC"
  echo "Build it first with: ./build.sh"
  exit 1
fi

# Default parametri 
DEFAULT_N=10000
DEFAULT_P=32
DEFAULT_A=seq
DEFAULT_T=4
DEFAULT_C=16384
DEFAULT_RANKS=4

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

# Default -t if missing and not seq
if ! has_flag "-t" && [[ "$algo" != "seq" ]]; then
  ARGS+=("-t" "$DEFAULT_T")
fi

# Default -c if missing and algo ff
if ! has_flag "-c" && [[ "$algo" == "ff" ]]; then
  ARGS+=("-c" "$DEFAULT_C")
fi

# MPI ranks via --np (come il tuo)
NP="$DEFAULT_RANKS"
for ((i=0; i<${#ARGS[@]}; i++)); do
  if [[ "${ARGS[i]}" == "--np" && $((i+1)) -lt ${#ARGS[@]} ]]; then
    NP="${ARGS[i+1]}"
    unset 'ARGS[i]'
    unset 'ARGS[i+1]'
    ARGS=("${ARGS[@]}")
    break
  fi
done

# --- CSV append helper (prende header + righe algo, evita header duplicati) ---
append_csv() {
  local tmp="$1"
  local dst="$2"

  # filtra solo righe CSV attese: header o "algo,..."
  awk -F',' '
    /^algo,/ {print; next}
    /^[a-z]+,[0-9]+,/ {print}
  ' "$tmp" > "$tmp.filtered"

  if [[ ! -s "$dst" ]]; then
    cat "$tmp.filtered" >> "$dst"
  else
    grep -v '^algo,' "$tmp.filtered" >> "$dst"
  fi
  rm -f "$tmp.filtered"
}

# --- launcher MPI: se sei sotto Slurm usa srun, altrimenti mpirun/mpiexec ---
run_mpi() {
  if [[ -n "${SLURM_JOB_ID:-}" ]]; then
    # -n = ranks
    srun -n "$NP" "$EXEC" "${ARGS[@]}"
  else
    MPI_LAUNCHER="${MPIEXEC:-mpirun}"
    if [[ "$MPI_LAUNCHER" =~ ^[[:space:]]*mpirun(\ |$) ]] || [[ "$MPI_LAUNCHER" =~ ^[[:space:]]*mpiexec(\ |$) ]]; then
      $MPI_LAUNCHER -np "$NP" "$EXEC" "${ARGS[@]}"
    else
      $MPI_LAUNCHER "$EXEC" "${ARGS[@]}"
    fi
  fi
}


if [[ -z "$APPEND_CSV" ]]; then
  if [[ "$algo" == "mpi" ]]; then
    run_mpi
  else
    "$EXEC" "${ARGS[@]}"
  fi
  exit 0
else
  tmp="$(mktemp)"
  if [[ "$algo" == "mpi" ]]; then
    ( run_mpi ) &> "$tmp"
  else
    ( "$EXEC" "${ARGS[@]}" ) &> "$tmp"
  fi
  append_csv "$tmp" "$APPEND_CSV"
  rm -f "$tmp"
  echo "[ok] appended CSV -> $APPEND_CSV"
fi