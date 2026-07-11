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

# add_library(syndicate ...) removed 2026-07-10 — confirmed zero consumers via
# `grep -rn "target_link_libraries.*syndicate" --include="CMakeLists.txt" .`
# across the whole repo. It bundled a second copy of every daemon .c file
# (granitord, bebopd, ratkingd, shredderd, splinterd, etc.) into a static
# library nothing ever linked against — pure wasted compile time on every
# full rebuild. Daemons build correctly as their own executables via
# src/daemon/CMakeLists.txt; this was dead weight, not infrastructure.
# SYNDICATE_SRC left defined above in case anyone wants to revive it
# deliberately with a real consumer in mind.
