#!/data/data/com.termux/files/usr/bin/bash

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PIPES="$BASE/pipes"

mkdir -p "$PIPES"

for p in superhero.pipe judgement.pipe execution.pipe escalation.pipe; do
    [ -p "$PIPES/$p" ] || mkfifo "$PIPES/$p"
done

echo "✔ Core installed (pipes ready)"
