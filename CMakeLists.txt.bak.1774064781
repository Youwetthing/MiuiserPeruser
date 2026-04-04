cmake_minimum_required(VERSION 3.10)
project(MiuiserPeruser C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Core libraries (if present)
if(EXISTS "${CMAKE_SOURCE_DIR}/src/core/CMakeLists.txt")
    add_subdirectory(src/core)
endif()

# TMNT daemon suite
add_subdirectory(src/daemon)

# Optional: only include tools if they contain a CMakeLists.txt
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt")
    add_subdirectory(tools)
endif()

# Optional: only include java-helper if it contains a CMakeLists.txt
if(EXISTS "${CMAKE_SOURCE_DIR}/java-helper/CMakeLists.txt")
    add_subdirectory(java-helper)
endif()

