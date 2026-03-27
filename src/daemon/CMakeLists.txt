# TMNT DAEMON BUILD SYSTEM — CLEAN, SPLINTER‑FREE EDITION
function(add_tmnt_daemon NAME)
    add_executable(${NAME}
        ${NAME}.c
        ipc.c
        daemon_core.c
    )

    target_link_libraries(${NAME}
        sensei_core
        pthread
    )
endfunction()

# IPC Library (Krang)
add_library(krang STATIC
    ipc.c
    krang.c
)

target_include_directories(krang PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(krang PUBLIC sensei_core)

# ----------------------------
# TMNT DAEMONS
# ----------------------------
add_tmnt_daemon(splinterd)


add_tmnt_daemon (metalhead)

add_tmnt_daemon (leatherheadd)

add_tmnt_daemon (bebopd)

add_tmnt_daemon (rocksteadyd)

add_tmnt_daemon (ratkingd)

add_tmnt_daemon (shredderd)

add_tmnt_daemon (fugitoidd)

add_tmnt_daemon (krangd)
target_sources(krangd PRIVATE krang.c)


add_tmnt_daemon (turtlecomd)


# Splinter Selector stays as a header-only helper (NOT a build target)
# splinter_selector.c / .h are included by daemons but not compiled as a target

