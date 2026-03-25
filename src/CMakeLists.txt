set(SRC_BACKENDS
    backends/backend_selector.c
    backends/backend_shizuku.c
    backends/backend_april.c
    # backends/backend_rish.c
    # backends/backend_shizuku.c
    # backends/backend_adb.c
)

set(SRC_SUPERHERO
    superhero/superhero_mode.c
)

add_library(mp_backends STATIC ${SRC_BACKENDS})
add_library(mp_superhero STATIC ${SRC_SUPERHERO})

add_executable(mp_superhero_scan main_superhero.c)
target_link_libraries(mp_superhero_scan mp_backends mp_superhero)
