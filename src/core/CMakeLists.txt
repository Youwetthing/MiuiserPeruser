cmake_minimum_required(VERSION 3.16)
project(sensei_core C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#
# ────────────────────────────────────────────────────────────────
#  Collect all core sources
# ────────────────────────────────────────────────────────────────
#

file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

#
# ────────────────────────────────────────────────────────────────
#  Sensei Core brain sources
# ────────────────────────────────────────────────────────────────
#

set(SENSEI_CORE_BRAIN
    ${CMAKE_SOURCE_DIR}/sensei_core.c
    ${CMAKE_SOURCE_DIR}/sensei_core/protocol.c
    ${CMAKE_SOURCE_DIR}/sensei_core/routing.c
    ${CMAKE_SOURCE_DIR}/sensei_core/sequence.c
    ${CMAKE_SOURCE_DIR}/sensei_core/exceptions.c
    ${CMAKE_SOURCE_DIR}/sensei_core/commands.c
    ${CMAKE_SOURCE_DIR}/sensei_core/knowledge.c
    ${CMAKE_SOURCE_DIR}/sensei_core/protection.c
    ${CMAKE_SOURCE_DIR}/sensei_core/analytics.c
)

#
# ────────────────────────────────────────────────────────────────
#  Build static library
# ────────────────────────────────────────────────────────────────
#

add_library(sensei_core STATIC
    ${CORE_SOURCES}
    ${SENSEI_CORE_BRAIN}
)

target_include_directories(sensei_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)

find_package(OpenSSL REQUIRED)

target_link_libraries(sensei_core
    OpenSSL::Crypto
)
