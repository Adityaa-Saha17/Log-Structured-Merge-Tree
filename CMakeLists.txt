cmake_minimum_required(VERSION 3.10)
project(LSMTree)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp" "main.cpp")

# Executable
add_executable(lsm_runner ${SOURCES})

# Add any necessary flags
if(MSVC)
    target_compile_options(lsm_runner PRIVATE /W4)
else()
    target_compile_options(lsm_runner PRIVATE -Wall -Wextra -O3)
endif()
