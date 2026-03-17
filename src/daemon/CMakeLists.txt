# ============================================================
# MiuiserPeruser Daemon
# ============================================================

set(SENSEI_DAEMON_SOURCES
    main.c
    service.c
    daemon_core.c
    ipc.c
    rish_pipe.c
)

add_executable(miuiserperuser-daemon ${SENSEI_DAEMON_SOURCES})

target_include_directories(miuiserperuser-daemon
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core/include
)

find_package(OpenSSL REQUIRED)
target_link_libraries(miuiserperuser-daemon
    PRIVATE
        sensei_core
        crypto
)

if(WIN32)
    target_link_libraries(miuiserperuser-daemon
        PRIVATE
            advapi32
    )
endif()

install(TARGETS miuiserperuser-daemon
    RUNTIME DESTINATION bin
)
