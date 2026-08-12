cmake_minimum_required(VERSION 3.10)

add_library(sensei_core STATIC
    april_table.c
    fugitoid_log_impl.c
    log_safe.c
    resource_snapshot.c
    sqlite/sqlite3.c
    detection/casey_hook.c
    detection/casey_input.c
    detection/casey_kernel.c
    detection/don_behavior.c
    detection/don_integrity.c
    detection/don_memorypressure.c
    detection/leo_detection.c
    detection/mikey_miui.c
    detection/raph_memory.c
    detection/raph_network.c
    event/april_event.c
    log/april_log.c
    platform/april_common.c
    platform/april_linux.c
    platform/capabilities.c
    platform/rish_pipe.c
    platform/shell_bridge.c
    ${CMAKE_SOURCE_DIR}/src/gaveld/mitre_map.c
)

add_executable(splinterd splinterd.c)
target_include_directories(splinterd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(splinterd PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_include_directories(sensei_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/detection
    ${CMAKE_CURRENT_SOURCE_DIR}/event
    ${CMAKE_CURRENT_SOURCE_DIR}/log
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_SOURCE_DIR}/src/superhero
    ${CMAKE_SOURCE_DIR}/include/backends
    ${CMAKE_SOURCE_DIR}/src/gaveld
)
