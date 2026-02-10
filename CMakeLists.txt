cmake_minimum_required(VERSION 3.16)
project(spm_project2 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer")

add_executable(sorter src/main.cpp src/io.cpp src/index.cpp src/utils.cpp)

#includes 
target_include_directories(sorter PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
#FastFlow 
target_include_directories(sorter PRIVATE
  ${CMAKE_SOURCE_DIR}/fastflow
)
# OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(sorter PRIVATE OpenMP::OpenMP_CXX)


