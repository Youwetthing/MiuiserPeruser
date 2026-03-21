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
