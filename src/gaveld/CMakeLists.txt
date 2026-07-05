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
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sqlite/sqlite3.c
)

target_compile_options(gaveld PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -std=c11
    -O2
    # SQLite compile-time options
    -DSQLITE_THREADSAFE=2           # serialised — we handle locking in db.c
    -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    -DSQLITE_OMIT_LOAD_EXTENSION    # no dlopen
)

target_include_directories(gaveld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../daemon    # backend_exec.h if needed
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
)

target_link_libraries(gaveld PRIVATE pthread)

set_target_properties(gaveld PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
