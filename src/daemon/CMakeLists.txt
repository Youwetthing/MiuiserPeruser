set(POWERHOUSE_SOURCES
    powerhouse_daemon.c
    rocksteady_parser.c
    leatherheadd_parser.c
    metalheadd_parser.c
    bebopd_parser.c
    ratkingd_parser.c
    shredderd_parser.c
    fugitoidd_parser.c
    krangd_parser.c
    splinterd_parser.c
)
add_executable(powerhouse_daemon ${POWERHOUSE_SOURCES})
target_include_directories(powerhouse_daemon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/src/core)
target_link_libraries(powerhouse_daemon sensei_core m)
