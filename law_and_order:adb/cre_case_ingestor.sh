#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

DISPATCH="$BASE/src/core/court_dispatcher.sh"
LAW="$BASE/law_and_order:adb"

emit_case() {
    entity="$1"
    severity="$2"
    reason="$3"

    bash "$DISPATCH" "$entity" "$severity" "$reason"
}

emit_case "$1" "$2" "$3"
