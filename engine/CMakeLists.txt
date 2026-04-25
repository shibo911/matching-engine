cmake_minimum_required(VERSION 3.15)
project(MatchingEngine VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Enable aggressive optimization for mechanical sympathy
# Note: On Windows with MSVC, these flags will be ignored or warned, 
# so we add conditional checking if using GCC/Clang (like in Docker later).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -flto -Wall -Wextra")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /std:c++20 /W4")
endif()

include_directories(include)

add_executable(engine 
    src/main.cpp 
    src/memory/EngineMemory.cpp
)

# ========================================================================
# Phase 4: Google Benchmark
# ========================================================================
include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(engine_benchmarks
    src/benchmarks.cpp
    src/memory/EngineMemory.cpp
)
target_link_libraries(engine_benchmarks PRIVATE benchmark::benchmark benchmark::benchmark_main)

add_executable(engine_latency
    src/latency_histogram.cpp
    src/memory/EngineMemory.cpp
)
