include_directories(${CMAKE_CURRENT_SOURCE_DIR})
cmake_minimum_required(VERSION 3.16)
project(sensei_core C)

# Collect all .c files recursively
file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

add_library(sensei_core STATIC ${CORE_SOURCES})

target_include_directories(sensei_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(OpenSSL REQUIRED)

target_link_libraries(sensei_core
    OpenSSL::Crypto
)
