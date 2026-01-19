cmake_minimum_required(VERSION 3.16)
project(spm_project2 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(seq src/main_seq.cpp src/io.cpp src/index.cpp)
add_executable(omp src/main_omp.cpp src/io.cpp src/index.cpp)
add_executable(ff  src/main_ff.cpp  src/io.cpp src/index.cpp)
add_executable(mpi src/main_mpi.cpp src/io.cpp src/index.cpp)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
  target_link_libraries(omp PRIVATE OpenMP::OpenMP_CXX)
  target_link_libraries(mpi PRIVATE OpenMP::OpenMP_CXX)
endif()
