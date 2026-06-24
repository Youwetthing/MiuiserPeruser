add_executable(rahzerd
    ${CMAKE_SOURCE_DIR}/src/daemon/rahzerd.c
    ${CMAKE_SOURCE_DIR}/src/daemon/ipc_globals.c
    ${CMAKE_SOURCE_DIR}/src/daemon/backend_exec.c
    ${CMAKE_SOURCE_DIR}/src/daemon/daemon_common.c
    ${CMAKE_SOURCE_DIR}/src/core/log_safe.c
    ${CMAKE_SOURCE_DIR}/src/core/fugitoid_log_impl.c
)

target_include_directories(rahzerd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/daemon
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/gaveld
)

target_link_libraries(rahzerd PRIVATE crypto)
set_target_properties(rahzerd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
