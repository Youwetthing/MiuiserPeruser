cmake_minimum_required(VERSION 3.10)

add_executable(superhero
    ${CMAKE_SOURCE_DIR}/src/main_superhero.c
    superhero_mode.c
    splinter_dojo.c
    backend_strategy.c
)

target_include_directories(superhero PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/superhero
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/core/include
    ${CMAKE_SOURCE_DIR}/src/core/sqlite
    ${CMAKE_SOURCE_DIR}/src/core/detection
    ${CMAKE_SOURCE_DIR}/src/core/platform
    ${CMAKE_SOURCE_DIR}/src/core/event
    ${CMAKE_SOURCE_DIR}/src/core/log
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(superhero PRIVATE sensei_core)

set_target_properties(superhero PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
