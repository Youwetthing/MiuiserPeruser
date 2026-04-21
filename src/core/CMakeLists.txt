cmake_minimum_required(VERSION 3.16)
project(sensei_core C)

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
src/core/april_runtime.c
)

add_library(sensei_core STATIC
    ${CORE_SOURCES}
)

target_include_directories(sensei_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/superhero
    ${CMAKE_SOURCE_DIR}/src/superhero/headers
     ${CMAKE_SOURCE_DIR}/src/syndicate
)

find_package(OpenSSL REQUIRED)

target_link_libraries(sensei_core
    OpenSSL::Crypto
)

target_include_directories(sensei_core PRIVATE
    ${CMAKE_SOURCE_DIR}
)
