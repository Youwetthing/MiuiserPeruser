add_executable(sar_engine sar_engine.c)
target_include_directories(sar_engine PRIVATE include)
set_target_properties(sar_engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
