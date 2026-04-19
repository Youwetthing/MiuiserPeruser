add_executable(rahzerd
    rahzerd.c
    modules/radio.c
    modules/environment.c
)

target_include_directories(rahzerd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)
