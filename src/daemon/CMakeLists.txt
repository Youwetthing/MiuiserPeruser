set(DAEMON_COMMON
    daemon_core.c
    daemon_common.c
    daemon_modes.c
    ipc_globals.c
    capabilities_extra.c
    capabilities_pretty.c
    ../core/log_safe.c
    ../core/fugitoid_log_impl.c
    backend_exec.c
)

set(DAEMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/daemon
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/gaveld
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/syndicate/daemon
)

foreach(DAEMON bebopd burned fugitoidd granitord krangd leatherheadd metalheadd ratkingd rocksteadyd shredderd turtlecomd)
    add_executable(${DAEMON} ${DAEMON}.c ${DAEMON_COMMON})
    target_include_directories(${DAEMON} PRIVATE ${DAEMON_INCLUDES})
    set_target_properties(${DAEMON} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endforeach()

# shredderd needs OpenSSL for SHA256
target_link_libraries(shredderd PRIVATE crypto)
target_link_libraries(granitord PRIVATE crypto)
