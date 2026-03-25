# TMNT DAEMON BUILD SYSTEM — CLEAN, SPLINTER‑FREE EDITION

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
add_executable(splinterd splinterd.c)
target_link_libraries(splinterd krang)

add_executable(metalhead metalhead.c)
target_link_libraries(metalhead PRIVATE sensei_core krang)

add_executable(leatherheadd leatherheadd.c)
target_link_libraries(leatherheadd PRIVATE sensei_core krang)

add_executable(bebopd bebopd.c)
target_link_libraries(bebopd PRIVATE sensei_core krang)

add_executable(rocksteadyd rocksteadyd.c)
target_link_libraries(rocksteadyd PRIVATE sensei_core krang)

add_executable(ratkingd ratkingd.c)
target_link_libraries(ratkingd PRIVATE sensei_core krang)

add_executable(shredderd shredderd.c)
target_link_libraries(shredderd PRIVATE sensei_core krang)

add_executable(fugitoidd fugitoidd.c)
target_link_libraries(fugitoidd PRIVATE sensei_core krang)

add_executable(krangd krangd.c)
target_link_libraries(krangd krang)

add_executable(turtlecomd turtlecomd.c)
# Splinter Selector stays as a header-only helper (NOT a build target)
# splinter_selector.c / .h are included by daemons but not compiled as a target

