cmake_minimum_required(VERSION 3.16)
project(memforge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(MEMFORGE_OPENMP "Parallelize with OpenMP (falls back to std::thread)" ON)
option(MEMFORGE_MPI "Build the distributed memforge-mpi driver if MPI is found" ON)
option(MEMFORGE_NATIVE "Tune for the build machine (-march=native)" OFF)

find_package(Threads REQUIRED)

add_library(memforge_core STATIC
  src/image.cpp
  src/finding.cpp
  src/options.cpp
  src/engine.cpp
  src/batch.cpp
  src/addrspace.cpp
  src/aho_corasick.cpp
  src/registry.cpp
  src/synth.cpp
  src/plugins/strings.cpp
  src/plugins/patscan.cpp
  src/plugins/netioc.cpp
  src/plugins/pescan.cpp
  src/plugins/entropy.cpp
  src/plugins/pooltag.cpp
  src/plugins/dtbscan.cpp)
target_include_directories(memforge_core PUBLIC include PRIVATE src)
target_compile_options(memforge_core PRIVATE -Wall -Wextra)
target_link_libraries(memforge_core PUBLIC Threads::Threads)
if(MEMFORGE_NATIVE)
  target_compile_options(memforge_core PUBLIC -march=native)
endif()

if(MEMFORGE_OPENMP)
  find_package(OpenMP COMPONENTS CXX)
  if(OpenMP_CXX_FOUND)
    target_link_libraries(memforge_core PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(memforge_core PRIVATE MEMFORGE_HAVE_OPENMP)
  else()
    message(STATUS "OpenMP not found: using the std::thread backend")
  endif()
endif()

add_executable(memforge src/main.cpp src/cli_common.cpp)
target_link_libraries(memforge PRIVATE memforge_core)
target_compile_options(memforge PRIVATE -Wall -Wextra)

add_executable(mkimage tools/mkimage.cpp)
target_link_libraries(mkimage PRIVATE memforge_core)

add_executable(memforge_tests tests/tests.cpp)
target_link_libraries(memforge_tests PRIVATE memforge_core)
target_compile_options(memforge_tests PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME unit COMMAND memforge_tests)
add_test(NAME cli COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_test.sh $<TARGET_FILE_DIR:memforge>)

if(MEMFORGE_MPI)
  find_package(MPI COMPONENTS CXX)
  if(MPI_CXX_FOUND)
    add_executable(memforge-mpi src/mpi_main.cpp src/cli_common.cpp)
    target_link_libraries(memforge-mpi PRIVATE memforge_core MPI::MPI_CXX)
    target_compile_options(memforge-mpi PRIVATE -Wall -Wextra)
    add_test(NAME mpi COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/mpi_test.sh
             $<TARGET_FILE_DIR:memforge> ${MPIEXEC_EXECUTABLE})
  else()
    message(STATUS "MPI not found: memforge-mpi will not be built")
  endif()
endif()
