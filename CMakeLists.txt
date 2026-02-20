cmake_minimum_required(VERSION 3.16)
project(spm_project2 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Opzione: abilita sanitizer in Debug (default ON)
option(ENABLE_ASAN "Enable sanitizers (ASan/UBSan) in Debug builds" ON)

# Pacchetti
find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(sorter
  src/main.cpp
  src/io.cpp
  src/index.cpp
  src/utils.cpp
  src/mpi_merge.cpp
)

# Include
target_include_directories(sorter PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/fastflow
  ${MPI_CXX_INCLUDE_DIRS}
  .
)

# Link
target_link_libraries(sorter PRIVATE
  MPI::MPI_CXX
  OpenMP::OpenMP_CXX
)

# Sanitizers SOLO in Debug
if (ENABLE_ASAN)
  target_compile_options(sorter PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer -O0 -g>
  )
  target_link_options(sorter PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )

  message(STATUS "Sanitizers requested: ENABLE_ASAN=ON (will apply only to Debug)")
else()
  message(STATUS "Sanitizers disabled: ENABLE_ASAN=OFF")
endif()