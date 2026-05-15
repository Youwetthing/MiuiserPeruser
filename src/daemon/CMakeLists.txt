cmake_minimum_required(VERSION 3.10)

# ── Shared compile options ─────────────────────────────────────────────
set(DAEMON_CFLAGS -Wall -Wextra -Wno-unused-parameter -std=c99)

# Source files shared by every daemon executable
set(DAEMON_COMMON_SRCS
    daemon_core.c
    ipc_globals.c
    daemon_common.c
)

# Include dirs for the daemon layer (can see src/core headers too)
set(DAEMON_INCDIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

# ── Helper macro ───────────────────────────────────────────────────────
# Usage:  syndicate_daemon(<target> <main-src> [extra-srcs...])
macro(syndicate_daemon name)
    add_executable(${name} ${ARGN} ${DAEMON_COMMON_SRCS})
    target_compile_options(${name} PRIVATE ${DAEMON_CFLAGS})
    target_include_directories(${name} PRIVATE ${DAEMON_INCDIRS})
endmacro()

# ── Tier-1: IPC hub ────────────────────────────────────────────────────
# turtlecomd — central Unix-socket broadcast hub; all reporters connect here
syndicate_daemon(turtlecomd  turtlecomd.c)

# krangd — normalise/arbitrate inbound payloads, forward to dashboard
# Depends on truth_engine from src/core/
add_executable(krangd
    krangd.c
    krang.c
    ${DAEMON_COMMON_SRCS}
    ../core/truth_engine.c
)
target_compile_options(krangd  PRIVATE ${DAEMON_CFLAGS})
target_include_directories(krangd PRIVATE ${DAEMON_INCDIRS})

# ── Tier-2: Reporters (connect to turtlecom, push STATUS blobs) ────────
# burned     — MIUI/HyperOS policy watcher (props: powerkeeper, bg data…)
syndicate_daemon(burned      burned.c)

# granitord  — security posture: SELinux, verified boot, root presence
syndicate_daemon(granitord   granitord.c)

# shredderd  — kernel integrity: verified boot, Magisk, su, flash lock
syndicate_daemon(shredderd   shredderd.c)

# ── Tier-3: Standalone monitors (daemon_core lifecycle, log to stdout) ─
# bebopd     — wake locks & pending alarms
syndicate_daemon(bebopd      bebopd.c)

# fugitoidd  — ADB/RISH bridge, activity & log stream monitor
syndicate_daemon(fugitoidd   fugitoidd.c)

# leatherheadd — real skin temperature vs HAL reported temp
syndicate_daemon(leatherheadd leatherheadd.c)

# metalheadd — sensor service: accelerometer, gyro, proximity
syndicate_daemon(metalheadd  metalheadd.c)

# rocksteadyd — CPU frequency & throttling watchdog
syndicate_daemon(rocksteadyd rocksteadyd.c)

# ratkingd   — zombie process & CPU-hog hunter
syndicate_daemon(ratkingd    ratkingd.c)

# ── Tier-4: Full-spectrum connectivity daemon ──────────────────────────
# rahzerd v2 — 15-layer connectivity audit; emits APRIL events to splinterd
add_executable(rahzerd
    rahzerd.c
    ${DAEMON_COMMON_SRCS}
)
target_compile_options(rahzerd  PRIVATE ${DAEMON_CFLAGS})
target_include_directories(rahzerd PRIVATE ${DAEMON_INCDIRS})
# rahzerd uses getaddrinfo → needs resolver library on some platforms
target_link_libraries(rahzerd PRIVATE -lpthread)
