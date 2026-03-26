# ---- Backend sources ----
set(SRC_BACKENDS
    backend/backend_april.c
    backend/backend_selector.c
    backend/backend_shizuku.c
    backend/sysport.c
)

# ---- Superhero sources ----
set(SRC_SUPERHERO
    superhero/superhero_mode.c
)

# ---- Backend library ----
add_library(mp_backends STATIC
    ${SRC_BACKENDS}
)

# ---- Superhero library ----
add_library(mp_superhero
    superhero/superhero_mode.c
    superhero/splinter_dojo.c
    # other files…
)

# ---- Executable ----
add_executable(mp_superhero_scan
    main_superhero.c
)

# ---- Linking ----
target_link_libraries(mp_superhero_scan
    mp_backends
    mp_superhero
)
