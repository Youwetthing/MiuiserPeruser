#!/data/data/com.termux/files/usr/bin/bash
# tool_backend.sh - privileged command helper (sourced by tools).
# Order: rish -> adb_cli local shell -> direct (unprivileged, warns on stderr).
export RISH_APPLICATION_ID=com.termux
RISH=""
_backend=""

_probe_backend() {
    [ -n "$_backend" ] && return
    local r
    for r in "$HOME/Rish/rish" "$HOME/rish"; do
        [ -x "$r" ] || continue
        if timeout 20 "$r" -c 'echo ok' 2>/dev/null | grep -q '^ok$'; then
            RISH="$r"; _backend="rish"; return
        fi
    done
    if command -v adb_cli >/dev/null 2>&1 && \
       timeout 20 adb_cli local shell 'echo ok' 2>/dev/null | grep -q '^ok'; then
        _backend="adb"; return
    fi
    _backend="direct"
}

backend_name() { _probe_backend; echo "$_backend"; }

privileged() {
    _probe_backend
    local out rc
    case "$_backend" in
        rish)
            out=$(timeout 30 "$RISH" -c "$*" 2>/dev/null); rc=$?
            [ -n "$out" ] && printf '%s\n' "$out" | sed '/^Waiting for Shizuku/d'
            return $rc ;;
        adb)
            timeout 30 adb_cli local shell "$*" 2>/dev/null ;;
        *)
            echo "[tool_backend] WARN: no privileged backend, running unprivileged: $*" >&2
            sh -c "$*" 2>/dev/null ;;
    esac
}

read_file() { cat "$1" 2>/dev/null || privileged "cat $1"; }
