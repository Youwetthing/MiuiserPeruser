# REMOVE legacy garbage from build

set(SYNDICATE_SRC
    backend/backend_exec.c
    backend/syndicate_run.c

    daemon_common.c
    daemon_core.c
    daemon_entry.c
    daemon_modes.c

    ipc.c
    ipc_globals.c

    # core daemons
    bebopd.c
    burned.c
    granitord.c
    krangd.c
    leatherheadd.c
    metalheadd.c
    ratkingd.c

    # splinter daemon
    splinterd.c
)

add_library(syndicate ${SYNDICATE_SRC})
