cmake_minimum_required(VERSION 3.10)
project(MiuiserPeruser C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_SUPERHERO "Build Superhero Mode scanner" ON)

# Global include path
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/include
)

#
# ────────────────────────────────────────────────────────────────
#  Self‑healing guard system
# ────────────────────────────────────────────────────────────────
#

message(STATUS "Running MiuiserPeruser self‑healing guard...")

execute_process(
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/miuiser_guard.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GUARD_RESULT
)

if(NOT GUARD_RESULT EQUAL 0)
    message(FATAL_ERROR "Self‑healing guard failed. See output above.")
endif()

message(STATUS "Guard completed successfully.")

#
# ────────────────────────────────────────────────────────────────
#  Subdirectories
# ────────────────────────────────────────────────────────────────
#

# Core library
add_subdirectory(src/core)

# Daemon components
add_subdirectory(src/daemon)

# Main backend + superhero build
add_subdirectory(src)

