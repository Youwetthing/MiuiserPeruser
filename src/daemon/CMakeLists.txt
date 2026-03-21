# src/daemon/CMakeLists.txt - daemon_common with weak-stub fallback

# Base sources for daemon_common
set(DAEMON_COMMON_SRCS
  daemon_common.c
  stubs.c
)

# Append any device-specific override files if present
foreach(f miui_xiaomi.c miui_redmi.c miui_generic.c)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
    list(APPEND DAEMON_COMMON_SRCS ${f})
  endif()
endforeach()

add_library(daemon_common STATIC ${DAEMON_COMMON_SRCS})
target_include_directories(daemon_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Example daemon targets in this directory
add_executable(burne burne.c)
target_link_libraries(burne PRIVATE daemon_common)

# Add other daemons here:
# add_executable(rocksteady rocksteady.c)
# target_link_libraries(rocksteady PRIVATE daemon_common)

# Prevent local duplicate logger implementations from being compiled into daemon targets.
list(REMOVE_ITEM DAEMON_COMMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/fugitoid_log.c)
list(REMOVE_ITEM DAEMON_COMMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/worker_log.c)

# Long-running daemon target for miuiserperuser
set(MIUISERPERUSER_SRCS
  main.c
  daemon_core.c
  ipc.c
  daemon_common.c
  miui_xiaomi.c
)

add_executable(miuiserperuser \${MIUISERPERUSER_SRCS})
target_include_directories(miuiserperuser PRIVATE \${CMAKE_SOURCE_DIR}/src \${CMAKE_SOURCE_DIR}/src/core \${CMAKE_SOURCE_DIR}/src/core/include)
target_link_libraries(miuiserperuser PRIVATE sensei_core daemon_common pthread dl)
