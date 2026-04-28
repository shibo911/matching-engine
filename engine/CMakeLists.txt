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
    src/gateway/ConnectionManager.cpp
    src/gateway/WebSocketGateway.cpp
)

# ========================================================================
# Phase 5: WebSockets Gateway (CrowCpp & ASIO for backward compatibility)
# ========================================================================
include(FetchContent)

FetchContent_Declare(
  asio
  GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
  GIT_TAG        asio-1-28-0
)
FetchContent_MakeAvailable(asio)

set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include CACHE PATH "ASIO include directory" FORCE)

FetchContent_Declare(
  Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG        v1.2.0
)
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "Disable Crow examples" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "Disable Crow tests" FORCE)
FetchContent_MakeAvailable(Crow)

include_directories(${ASIO_INCLUDE_DIR})

target_link_libraries(engine PRIVATE Crow::Crow)

# ========================================================================
# Phase 5: uWebSockets Implementation (New high-performance gateway)
# ========================================================================
option(UWS_ENABLE "Enable uWebSockets support" ON)

if(UWS_ENABLE)
    message(STATUS "uWebSockets enabled")
    FetchContent_Declare(
      uWebSockets
      GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
      GIT_TAG        v20.30.0
    )
    set(UWS_BUILD_TESTS OFF CACHE BOOL "Disable uWebSockets tests" FORCE)
    set(UWS_WITH_OPENSSL OFF CACHE BOOL "Disable SSL for minimal footprint" FORCE)
    set(UWS_WITH_ZLIB OFF CACHE BOOL "Disable compression for lowest latency" FORCE)
    FetchContent_MakeAvailable(uWebSockets)

    target_link_libraries(engine PRIVATE uWebSockets::uWebSockets)
    target_compile_definitions(engine PRIVATE UWS_ENABLE=1)
else()
    message(STATUS "uWebSockets disabled")
    target_compile_definitions(engine PRIVATE UWS_ENABLE=0)
endif()

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
