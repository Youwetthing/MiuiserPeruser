set(SYNDICATE_SRC
    backend/backend_exec.c
    backend/syndicate_run.c
    backend/capabilities.c
    daemon_entry.c

    ../daemon/daemon_common.c
    ../daemon/daemon_core.c
    ../daemon/daemon_modes.c
    ../daemon/log.c
    ../daemon/ipc.c
    ../daemon/ipc_globals.c

    ../daemon/bebopd.c
    ../daemon/burned.c
    ../daemon/fugitoidd.c
    ../daemon/granitord.c
    ../daemon/krangd.c
    ../daemon/leatherheadd.c
    ../daemon/metalheadd.c
    ../daemon/ratkingd.c
    ../daemon/rocksteadyd.c
    ../daemon/shredderd.c
    ../daemon/turtlecomd.c

    ../core/splinterd.c
)

add_library(syndicate ${SYNDICATE_SRC})

target_include_directories(syndicate PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/daemon
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/syndicate/daemon
)
