# src/toolkit/CMakeLists.txt
#
# portscan — minimal loopback TCP port scanner used by resolve_adb_port()
# in backend_exec.c to find ADB's current wireless-debugging port
# instead of assuming a hardcoded :5555 (proven necessary the hard way:
# the port rotated multiple times in one night before this fix existed).
#
# No external deps beyond libc — deliberately kept out of DAEMON_COMMON
# (src/daemon/CMakeLists.txt) since it's a standalone toolkit binary, not
# a daemon, and doesn't need any of the shared daemon sources.

add_executable(portscan portscan.c)

set_target_properties(portscan PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
