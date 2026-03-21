#!/usr/bin/env bash
set -euo pipefail
APPLY=0
if [ "${1:-}" = "--apply" ]; then APPLY=1; shift; fi
FILES="${@:-src/daemon/*.c src/*.cpp}"
for f in $FILES; do
  [ -f "$f" ] || continue
  perl -0777 -pe '
    s/fugitoid_log\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*\)\s*;/fugitoid_log_json("$1","UNKNOWN_DOMAIN","UNKNOWN_COMPONENT","UNKNOWN_EVENT","", "$2", "{}");/gs;
  ' "$f" > /tmp/conv.$$ && diff -u "$f" /tmp/conv.$$ >/dev/null 2>&1 || {
    echo "=== Proposed changes for $f ==="
    diff -u "$f" /tmp/conv.$$ || true
    if [ "$APPLY" -eq 1 ]; then mv /tmp/conv.$$ "$f"; echo "Applied $f"; else rm /tmp/conv.$$; fi
  }
done
echo "Done."
