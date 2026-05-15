#!/data/data/com.termux/files/usr/bin/bash

DISPATCH="$HOME/MiuiserPeruser/src/core/court_dispatcher.sh"
LAW="$HOME/MiuiserPeruser/law_and_order:adb"

emit_case() {
    entity="$1"
    severity="$2"
    reason="$3"

    bash "$DISPATCH" "$entity" "$severity" "$reason"
}

emit_case "$1" "$2" "$3"
