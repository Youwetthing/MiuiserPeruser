# src/gaveld/CMakeLists.txt
# gaveld — Judicial Scoring Daemon
# Bundles SQLite amalgamation — no external -lsqlite3 needed

cmake_minimum_required(VERSION 3.10)

add_executable(gaveld
    gaveld.c
    ingest.c
    scorer.c
    cases.c
    verdict.c
    consent.c
    enforce.c
    audit.c
    decay.c
    db.c
    log.c
    tier.c
    weights.c
    mitre_map.c
)

target_compile_options(gaveld PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -std=c11
    -O2
)

target_include_directories(gaveld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemon
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

target_link_libraries(gaveld PRIVATE pthread)

set_target_properties(gaveld PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

target_link_libraries(gaveld PRIVATE sqlite3_vendored)
