SPM Project 2 – Distributed out-of-core MergeSort

# Build:
./scripts/build.sh 

# Single scripts/run
./scripts/run.sh -a omp -n 200000 -p 256 -t 8

# Append su csv 
./scripts/run.sh --append-csv results/single.csv -a seq -n 200000 -p 256
./scripts/run.sh --append-csv results/single.csv -a omp -n 200000 -p 256 -t 8
./scripts/run.sh --append-csv results/mpi.csv --np 8 -a mpi -n 200000 -p 256 -t 4

# Test - con ripetizioni x media
## Single node scaling
result/single_node_scaling.csv 

for rep in 1 2 3; do
  ./scripts/run.sh --append-csv results/single_node_scaling.csv -a seq -n 200000 -p 256
done
for t in 1 2 4 8; do
      for rep in 1 2 3; do
            ./scripts/run.sh --append-csv results/single_node_scaling.csv -a omp -n 200000 -p 256 -t "$t"
      done
      for rep in 1 2 3; do
            ./scripts/run.sh --append-csv results/single_node_scaling.csv -a ff  -n 200000 -p 256 -t "$t" -c 16384
      done
done


## Size scaling 
results/size_scaling.csv

T=8
for n in 50000 100000 200000 500000 1000000; do
      for rep in 1 2 3; do
            ./scripts/run.sh --append-csv results/size_scaling.csv -a seq -n "$n" -p 256
      done 
      for rep in 1 2 3; do
            ./scripts/run.sh --append-csv results/size_scaling.csv -a omp -n "$n" -p 256 -t "$T"
      done
      for rep in 1 2 3; do
            ./scripts/run.sh --append-csv results/size_scaling.csv -a ff  -n "$n" -p 256 -t "$T" -c 16384
      done
done

## Payload sensitivity 
results/payload_sensitivity.csv

N=200000
T=8
for p in 0 8 32 256 1024; do
      for reps in 1 2 3; do
            ./scripts/run.sh --append-csv results/payload_sensitivity.csv -a seq -n "$N" -p "$p"
      done
      for reps in 1 2 3; do
            ./scripts/run.sh --append-csv results/payload_sensitivity.csv -a omp -n "$N" -p "$p" -t "$T"
      done
      for reps in 1 2 3; do
            ./scripts/run.sh --append-csv results/payload_sensitivity.csv -a ff  -n "$N" -p "$p" -t "$T" -c 16384
      done
done

## MPI local test 
results/mpi_local_test.csv 

export OMP_NUM_THREADS=1
for np in 1 2 3 4 5 6 7 8; do
      for reps in 1 2 3; do
            ./scripts/run.sh --append-csv results/mpi_local_test.csv --np 2 -a mpi -n 20000 -p 32 -t 1
      done 
      for reps in 1 2 3; do
            ./scripts/run.sh --append-csv results/mpi_local_test.csv --np 4 -a mpi -n 20000 -p 32 -t 1
      done 
done


# MPI local test fatto = > ora modificare codice e creare grafico 
Alorithms: 
-a seq
-a omp
-a ff