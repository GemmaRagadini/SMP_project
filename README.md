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

Alorithms: 
-a seq
-a omp
-a ff