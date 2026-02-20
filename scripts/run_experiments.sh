#!/usr/bin/env bash
set -euo pipefail

# usa run.sh in modalità Release
RUN_SH="${RUN_SH:-./scripts/run.sh}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

OUTDIR="${OUTDIR:-results}"
mkdir -p "$OUTDIR"

# ripetizioni
REPS=3

# per adesso solo questo caso base
N="${N:-10000000}"
P="${P:-32}"               # payload_max
CUTOFF="${CUTOFF:-10000}"
THREADS="${THREADS:-4}"

# MPI: due casi base
NP1="${NP1:-1}"
NP2="${NP2:-2}"
NP4="${NP4:-4}"

# file output
TS="$(date +%Y%m%d_%H%M%S)"
CSV_SINGLE="$OUTDIR/single_base_${TS}.csv"
CSV_MPI="$OUTDIR/mpi_base_${TS}.csv"
LOG="$OUTDIR/base_${TS}.log"

echo "[info] single CSV: $CSV_SINGLE" | tee -a "$LOG"
echo "[info] mpi    CSV: $CSV_MPI"    | tee -a "$LOG"
echo "[info] case: N=$N payload_max=$P cutoff=$CUTOFF threads=$THREADS" | tee -a "$LOG"
echo "[info] build type: $BUILD_TYPE" | tee -a "$LOG"
echo "[info] runner: $RUN_SH"         | tee -a "$LOG"

# header CSV
echo "algo,n,p,cutoff,threads,build_time,sort_time,write_time,check_time,total_time, t_c" > "$CSV_SINGLE"
echo "algo,n,p,cutoff,threads_per_rank,build_time,sort_time_max,merge_time_max,write_time,check_time,total_time_max,t_c,np" > "$CSV_MPI"

# Estrae solo la riga CSV che inizia con seq/omp/ff/mpi (scarta header e log)
extract_csv_line() {
  awk -F',' '
    $1=="algo"{next}
    $1~/^(seq|omp|ff|mpi)$/ {print; found=1}
    END{if(!found) exit 2}
  '
}

# esegue run single node e salva
run_single() {
  local algo="$1"
  local th="$2"

  echo "[single] algo=$algo th=$th" | tee -a "$LOG"
  "$RUN_SH" "$BUILD_TYPE" -a "$algo" -n "$N" -p "$P" -c "$CUTOFF" -t "$th" \
    2>&1 | tee -a "$LOG" | extract_csv_line >> "$CSV_SINGLE"
}

# run MPI e salva la riga CSV + aggiunge np
run_mpi() {
  local np="$1"
  local tpr="$2"

  echo "[mpi] np=$np tpr=$tpr" | tee -a "$LOG"
  # shellcheck disable=SC2086
  "$RUN_SH" "$BUILD_TYPE" --np "$np" -a mpi -n "$N" -p "$P" -c "$CUTOFF" -t "$tpr" \
    2>&1 | tee -a "$LOG" | extract_csv_line | awk -v np="$np" '{print $0","np}' >> "$CSV_MPI"
}

# -------------------------
# 1) Single-node base: seq + omp + ff
# -------------------------

THREAD_LIST=(1 2 4 8 16)

run_single "seq" 1

for th in "${THREAD_LIST[@]}"; do
  for r in $(seq 1 "$REPS"); do
    run_single "omp" "$th"
    run_single "ff"  "$th"
  done
done



# MPI
MPI_TPR=4
MPI_NP_LIST=(1 2 4)

for np in "${MPI_NP_LIST[@]}"; do
  for r in $(seq 1 "$REPS"); do
    run_mpi "$np" "$MPI_TPR"
  done
done