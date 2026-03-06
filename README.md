# Distributed out-of-core MergeSort (Index-based)

This project implements an **out-of-core MergeSort** for a binary dataset made of **variable-length records**.  
The dataset can exceed RAM; therefore records are **not sorted in-place**. Instead, the program builds an
**in-memory index** of `(key, offset, len)` tuples, sorts the index, and finally rewrites the output file
by copying records in sorted order.

The project includes:
- **Single-node** implementations:
  - `seq` : sequential baseline (top-down mergesort)
  - `omp` : OpenMP task-based mergesort with cutoff
  - `ff`  : FastFlow `ParallelFor` (block sort + iterative merges)
- **Hybrid distributed** implementation:
  - `mpi` : MPI + OpenMP range-partitioning (scatter + local sort + sampling splitters + alltoallv + local k-way merge)

## Data format

Each record in the input file is:
- `key` : `unsigned long` (8 bytes on 64-bit platforms) — sorting key
- `len` : `uint32_t` — payload length
- `payload[len]` : arbitrary bytes

The in-memory index is:
- `key`
- `offset` (byte offset of the record in the file)
- `len`

The comparator uses **(key, offset)** to ensure deterministic ordering with duplicate keys.

## Repository structure (typical)

- `scripts/build.sh` : configure + build with CMake
- `scripts/run.sh` : convenience wrapper for single-node runs + optional CSV append
- `scripts/experiments` : Slurm experiment scripts (single node scaling, strong/weak scaling, etc.)
- `build/` : build directory (default)
- `results/` : CSV outputs


> The executable produced by CMake is expected at: `build/sorter`

---

# Build and Execute

```bash
./scripts/build.sh

# Run

Common command-line parameters:

| Flag | Meaning |
|-----|--------|
| `-a` | algorithm (`seq`, `omp`, `ff`, `mpi`) |
| `-n` | number of records |
| `-p` | maximum payload size |
| `-t` | number of threads |

## Examples 

### Single-node execution

./build/sorter -a seq -n 1000000 -p 256

### OpenMP version 
export OMP_PLACES=cores
export OMP_PROC_BIND=close
export OMP_NUM_THREADS=16

./build/sorter -a omp -n 100000 -p 16 -t 16

### FastFlow version 
./build/sorter -a ff -n 100000 -p 16 -t 16

### Running using the wrapper script
The script `scripts/run.sh` provides a convenient interface and can also
append results to a CSV file.

Example:
./scripts/run.sh --append-csv results/test.csv -a ff -n 100000 -p 16 -t 16

### Running an experiment on cluster
Example: 
./scripts/experiments/single_node_test_cluster_A.sh  

Results will be in "results/" 