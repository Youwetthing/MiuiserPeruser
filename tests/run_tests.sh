#!/usr/bin/env bash
#
# run_tests.sh — build and run the MiuiserPeruser unit tests.
#
#   ./tests/run_tests.sh                 run the suite
#   ./tests/run_tests.sh --coverage      run instrumented, print line coverage
#   ./tests/run_tests.sh --system-sqlite link -lsqlite3 (faster than the
#                                        bundled amalgamation)
#
set -euo pipefail

cd "$(dirname "$0")"

COVERAGE=0
MAKE_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --coverage)      COVERAGE=1 ;;
        --system-sqlite) MAKE_ARGS+=("SQLITE=system") ;;
        -h|--help)       sed -n '3,10p' "$0"; exit 0 ;;
        *)               echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [ "$COVERAGE" = "1" ]; then
    MAKE_ARGS+=("COVERAGE=1")
    make clean >/dev/null
fi

make "${MAKE_ARGS[@]}" run
status=$?

if [ "$COVERAGE" = "1" ]; then
    command -v gcov >/dev/null || { echo "gcov not installed"; exit "$status"; }
    echo
    echo "== line coverage of modules under test"
    for objdir in build/obj/test_*/; do
        gcov -n -o "$objdir" "$objdir"*.gcno 2>/dev/null || true
    done | awk '
        /^File/ { file = $2; gsub(/'"'"'/, "", file) }
        /^Lines executed:/ {
            if (file ~ /src\/gaveld\//) {
                split($0, a, ":"); split(a[2], b, "%");
                pct = b[1] + 0;
                n = split(file, p, "/"); name = p[n];
                if (pct > best[name]) best[name] = pct
            }
        }
        END { for (f in best) printf "  %-16s %5.1f%%\n", f, best[f] }
    ' | sort
fi

exit "$status"
