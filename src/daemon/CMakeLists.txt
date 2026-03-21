# src/daemon/CMakeLists.txt — wired for miuiserperuser build

# --- Self-healing guard: required sources must exist ---
set(REQUIRED_DAEMON_SRCS
    daemon_common.c
    service.c
    rish_pipe.c
)

foreach(f ${REQUIRED_DAEMON_SRCS})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
        message(FATAL_ERROR "Required daemon source missing: ${f}. Build halted.")
    endif()
endforeach()

# Sources for the main miuiserperuser daemon
set(MIUISERPERUSER_SRCS
    main.c
    daemon_core.c
    ipc.c

    # Provides miuiserperuser_service_start()
    service.c

    # Provides rish_pipe_start/stop/command()
    rish_pipe.c
)

# Build daemon_common as a static library
add_library(daemon_common STATIC
    daemon_common.c
)

# Build the executable
add_executable(miuiserperuser ${MIUISERPERUSER_SRCS})

# Link against core libraries
target_link_libraries(miuiserperuser
    PRIVATE
        sensei_core
        daemon_common
        pthread
        dl
)
