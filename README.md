SPM Project 2 – Distributed out-of-core MergeSort

Build All: 
./scripts/build.sh All

Build (Release):
./scripts/build.sh Release

Build (Debug):
./scripts/build.sh Debug

Run (Release – default parameters):
./scripts/run.sh

Run (Debug – default parameters):
./scripts/run.sh Debug

Ex. Run with custom parameters (Release):
./scripts/run.sh Release -a ff -t 16 -c 10000

Ex. Run with custom parameters (Debug):
./scripts/run.sh Debug -a omp -t 4

# run esperimenti senza modificare file
BUILD_TYPE=Debug OUTDIR=myres ./scripts/run_experiments.sh
N=20000000 P=32 THREADS=16 ./scripts/run_experiments.sh

Alorithms: 
-a seq
-a omp
-a ff