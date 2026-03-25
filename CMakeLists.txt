cmake_minimum_required(VERSION 3.10)
project(MiuiserPeruser C)

set(CMAKE_C_STANDARD 99)

# Global include path so all modules see core headers
include_directories(
    ${CMAKE_SOURCE_DIR}/src/core/include
)

add_subdirectory(src/core)
add_subdirectory(src/daemon)

#
# ────────────────────────────────────────────────────────────────
#  Self‑healing guard system (runs automatically on cmake ..)
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

