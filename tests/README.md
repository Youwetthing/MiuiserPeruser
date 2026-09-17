# Unit tests

Host-runnable unit tests for the pure-logic parts of the tree. No test
framework dependency — `test_harness.h` is a handful of assertion macros and
each test file is a standalone binary that exits non-zero on failure.

```sh
./tests/run_tests.sh                  # build + run everything
./tests/run_tests.sh --coverage       # + per-module gcov line coverage
./tests/run_tests.sh --system-sqlite  # link -lsqlite3 instead of the bundled amalgamation
make -C tests clean
```

## What is covered

The gaveld judicial pipeline (`src/gaveld/`), which holds the scoring and
verdict logic and is the only daemon layer with no device dependency:

| test | module under test |
| --- | --- |
| `test_weights.c`   | signal weight / ATT&CK table lookup |
| `test_mitre_map.c` | ATT&CK Mobile enrichment map |
| `test_log.c`       | log open/append/rotation |
| `test_tier.c`      | source trust tiers (own daemon / sovereignty / system) |
| `test_ingest.c`    | `SOURCE\|SIGNAL\|WEIGHT\|CTX` parser and record ring buffer |
| `test_db.c`        | SQLite persistence layer |
| `test_cases.c`     | case assembly and verdict-queue routing |
| `test_scorer.c`    | weight × tier × covariance × recidivism scoring maths |

`test_ingest.c` `#include`s `ingest.c` directly because the parser and the ring
buffer are file-static; every other test links the module normally.

## Sandboxing

The tests compile the gaveld sources with `-DBASE_DIR=tests/build/root`, so
`config.h` derives `DB_PATH`, `LOG_PATH`, `SOVEREIGNTY_LIST` and the pipe paths
from a scratch directory. Nothing touches the on-device state under
`/data/data/com.termux/...`, and the FIFO/consent/verdict threads are never
started — only the logic they call into is exercised.

## Adding a test

1. Drop `tests/gaveld/test_<module>.c` next to the others, with a `main()` that
   calls `RUN_TEST()` and returns `test_report()`.
2. Add the module's sources to `SRC_test_<module>` in `tests/Makefile` and the
   name to `TESTS`.
