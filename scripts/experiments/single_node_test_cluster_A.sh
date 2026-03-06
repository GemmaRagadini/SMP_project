#!/usr/bin/env bash
set -euo pipefail

mkdir -p logs results

# SCENARIO A
N=100000000
PAYLOAD_MAX=16

CSV="results/single_node_test_A$(date +%Y%m%d_%H%M%S).csv"

# Fixed CPU budget for all runs 
CPUS_PER_TASK=32

run_one () {
    local bind="${1:?cpu-bind required (cores|none)}"
    shift

    srun --nodes=1 --ntasks=1 --cpus-per-task="$CPUS_PER_TASK" \
         --cpu-bind="$bind" --distribution=block:block \
         ./scripts/run.sh --append-csv "$CSV" "$@"
}

echo "Running single-node scaling on $(hostname)"
echo "Fixed allocation: cpus-per-task=$CPUS_PER_TASK"

#  SEQ baseline 
export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=1

run_one cores -a seq -n "$N" -p "$PAYLOAD_MAX"

#  OMP vs FF scaling
THREADS=(1 2 4 8 16 24 30 32)

for t in "${THREADS[@]}"; do
  #  OpenMP 
  export OMP_PLACES=cores
  export OMP_PROC_BIND=close
  export OMP_NUM_THREADS="$t"

  for rep in 1 2; do
    run_one cores -a omp -n "$N" -p "$PAYLOAD_MAX" -t "$t"
  done

  #  FastFlow
  unset OMP_NUM_THREADS OMP_PLACES OMP_PROC_BIND

  for rep in 1 2; do
    run_one none -a ff -n "$N" -p "$PAYLOAD_MAX" -t "$t"
  done
done

echo "DONE -> $CSV"