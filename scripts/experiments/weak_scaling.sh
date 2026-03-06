#!/usr/bin/env bash
set -euo pipefail

mkdir -p logs results

# Lavoro per rank fisso:
N_PER_RANK=5000000          
PAYLOAD_MAX=32
ALGO="mpi"

CORES_PER_NODE=32
NODES_LIST=(1 2 4 8)
RANKS_PER_NODE_LIST=(1 2 4 8)

REPS=2
TIME_LIMIT="${TIME_LIMIT:-00:10:00}"

EXEC="build/sorter"
if [[ ! -x "$EXEC" ]]; then
  echo "Executable not found: $EXEC" >&2
  echo "Build it first with: ./build.sh" >&2
  exit 1
fi

CSV="results/weak_${ALGO}_NPR${N_PER_RANK}_P${PAYLOAD_MAX}_$(date +%Y%m%d_%H%M%S).csv"

# aggiungo rpn
append_csv_with_rpn() {
  local tmp="$1"
  local dst="$2"
  local rpn="$3"

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

  awk -v rpn="$rpn" '
    BEGIN{FS=OFS=","}
    NR==1 && $1=="algo" {print $0,"rpn"; next}
    $1!="algo" {print $0,rpn}
  ' "$tmp.filtered" > "$tmp.filtered.plus"

  if [[ ! -s "$dst" ]]; then
    cat "$tmp.filtered.plus" >> "$dst"
  else
    awk -F',' '$1!="algo"{print}' "$tmp.filtered.plus" >> "$dst"
  fi

  rm -f "$tmp.filtered" "$tmp.filtered.plus"
}

run_one() {
  local nodes="$1"
  local rpn="$2"
  local tpr="$3"
  local ntasks=$((nodes * rpn))

  #  N totale cresce con ntasks
  local N_TOTAL=$((N_PER_RANK * ntasks))

  export OMP_NUM_THREADS="$tpr"
  export OMP_PLACES=cores
  export OMP_PROC_BIND=close

  echo "RUN nodes=$nodes rpn=$rpn tpr=$tpr ntasks=$ntasks N_TOTAL=$N_TOTAL"

  local tmp
  tmp="$(mktemp)"

  srun --nodes="$nodes" \
       --ntasks="$ntasks" \
       --ntasks-per-node="$rpn" \
       --cpus-per-task="$tpr" \
       --cpu-bind=cores \
       --distribution=block:block \
       --exclusive \
       --mpi=pmix \
       --time="$TIME_LIMIT" \
       "$EXEC" -a "$ALGO" -n "$N_TOTAL" -p "$PAYLOAD_MAX" -t "$tpr" \
       &> "$tmp" || {
         echo "[weak] sorter failed. Raw output:" >&2
         cat "$tmp" >&2
         rm -f "$tmp"
         exit 1
       }

  append_csv_with_rpn "$tmp" "$CSV" "$rpn"
  rm -f "$tmp"
  echo "[ok] appended CSV -> $CSV"
}

echo "=== WEAK SCALING ($ALGO) ==="
echo "Fixed work per rank: N_PER_RANK=$N_PER_RANK PAYLOAD_MAX=$PAYLOAD_MAX"
echo "CSV -> $CSV"

for nodes in "${NODES_LIST[@]}"; do
  echo "---- nodes=$nodes ----"
  for rpn in "${RANKS_PER_NODE_LIST[@]}"; do
    if (( CORES_PER_NODE % rpn != 0 )); then
      continue
    fi
    tpr=$(( CORES_PER_NODE / rpn ))

    for rep in $(seq 1 "$REPS"); do
      run_one "$nodes" "$rpn" "$tpr"
    done
  done
done

echo "DONE -> $CSV"