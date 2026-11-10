#!/data/data/com.termux/files/usr/bin/bash
# tool_backend.sh - privileged command helper (sourced by tools).
# Order: adb_cli local shell (fast) -> rish (2 tries) -> direct (unprivileged, warns).
export RISH_APPLICATION_ID=com.termux
ADB_CLI="$HOME/.cargo/bin/adb_cli"
RISH="$HOME/rish"
_backend=""

_probe_backend() {
    [ -n "$_backend" ] && return
    if [ -x "$ADB_CLI" ] && timeout 15 "$ADB_CLI" local shell 'echo ok' 2>/dev/null | grep -q '^ok'; then
        _backend="adb"; return
    fi
    if [ -x "$RISH" ]; then
        local i
        for i in 1 2; do
            if timeout 15 "$RISH" -c 'echo ok' 2>/dev/null | grep -q '^ok'; then
                _backend="rish"; return
            fi
        done
    fi
    _backend="direct"
}

backend_name() { _probe_backend; echo "$_backend"; }

privileged() {
    _probe_backend
    case "$_backend" in
        adb)  timeout 30 "$ADB_CLI" local shell "$*" 2>/dev/null ;;
        rish) timeout 30 "$RISH" -c "$*" 2>/dev/null | sed '/^Waiting for Shizuku/d' ;;
        *)
            echo "[tool_backend] WARN: no privileged backend, running unprivileged: $*" >&2
            sh -c "$*" 2>/dev/null ;;
    esac
}

read_file() { cat "$1" 2>/dev/null || privileged "cat $1"; }
