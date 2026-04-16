# Explicitly list the heavy hitters to ensure they aren't skipped by globbing
set(CORE_SRCS
    april_table.c
    fugitoid_log_impl.c
    log_safe.c
    resource_snapshot.c
    detection/leo_detection.c
    detection/casey_hook.c
    detection/casey_kernel.c
    detection/don_behavior.c
    detection/don_integrity.c
    detection/don_memorypressure.c
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
    platform/shell_ghost.c
    platform/sysport_ghost.c
)

# Add any backend files found in the core root
file(GLOB BACKEND_SRCS "backend_*.c")

add_library(sensei_core STATIC ${CORE_SRCS} ${BACKEND_SRCS})

target_include_directories(sensei_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/detection
    ${CMAKE_CURRENT_SOURCE_DIR}/event
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
)
