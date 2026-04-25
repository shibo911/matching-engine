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
