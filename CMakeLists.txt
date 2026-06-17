cmake_minimum_required(VERSION 3.16)
project(verify_kokkos CXX)

set(CMAKE_CXX_STANDARD 20)

# Point CMake to your installation prefix
find_package(Kokkos REQUIRED)

# Verification binary
add_executable(verify_kokkos verify_kokkos.cpp)
target_link_libraries(verify_kokkos Kokkos::kokkos)

# Your N-Queens binary
add_executable(queens_kokkos queens_kokkos.cpp)
target_link_libraries(queens_kokkos Kokkos::kokkos)