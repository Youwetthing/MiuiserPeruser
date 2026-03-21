cmake_minimum_required(VERSION 3.16)

set(SENSEI_CORE_SOURCES
    platform/april_common.c
    platform/april_linux.c
    event/april_event.c
    detection/leo_detection.c
    detection/raph_memory.c
    detection/raph_network.c
    detection/don_integrity.c
    detection/don_behavior.c
    detection/casey_hook.c
    detection/casey_kernel.c
    detection/mikey_miui.c
    detection/don_memorypressure.c
    log/april_log.c
)

add_library(sensei_core STATIC ${SENSEI_CORE_SOURCES})

target_include_directories(sensei_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/daemon   # Add this line
)

find_package(OpenSSL REQUIRED)
target_link_libraries(sensei_core
    PRIVATE
        crypto
)

# Ensure a single canonical C logger implementation is built into core
target_sources(sensei_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fugitoid_log_impl.c ${CMAKE_CURRENT_SOURCE_DIR}/resource_snapshot.c)
