#!/data/data/com.termux/files/usr/bin/bash
# tool_backend.sh - privileged command helper for tools (source it).
# Probes ONCE at source time, in TB_ORDER; first backend that answers wins.
# Then direct (unprivileged: most system info unreachable, warns on stderr).
# Override: TB_ORDER="rish adb"   TB_NO_PROBE=1 to probe lazily.
export RISH_APPLICATION_ID=com.termux
TB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TB_ORDER="${TB_ORDER:-adb rish adb_cli}"
TB_PORT_LO="${TB_PORT_LO:-30000}"
TB_PORT_HI="${TB_PORT_HI:-45000}"
ADB_CLI="$HOME/.cargo/bin/adb_cli"
RISH="$HOME/rish"
PORTSCAN="$TB_ROOT/bin/portscan"
_backend=""
_serial=""

_adb_ok() { timeout 10 adb -s "$1" shell 'echo ok' 2>/dev/null | tr -d '\r' | grep -q '^ok$'; }

_probe_adb() {
    command -v adb >/dev/null 2>&1 || return 1
    local s p
    s=$(timeout 10 adb devices 2>/dev/null | tr -d '\r' | awk 'NR>1 && $2=="device"{print $1; exit}') || true
    if [ -n "$s" ] && _adb_ok "$s"; then _serial="$s"; return 0; fi
    local cache="$TB_ROOT/pipes/state/.tb_adb_serial" c
    c=$(cat "$cache" 2>/dev/null || true)
    if [ -n "$c" ]; then
        timeout 6 adb connect "$c" >/dev/null 2>&1 || true
        _adb_ok "$c" && { _serial="$c"; return 0; }
    fi
    [ -x "$PORTSCAN" ] || return 1
    for p in $(timeout 30 "$PORTSCAN" "$TB_PORT_LO" "$TB_PORT_HI" 127.0.0.1 -a 2>/dev/null | head -5); do
        timeout 6 adb connect "127.0.0.1:$p" >/dev/null 2>&1 || true
        if _adb_ok "127.0.0.1:$p"; then _serial="127.0.0.1:$p"; { echo "$_serial" > "$cache"; } 2>/dev/null || true; return 0; fi
    done
    return 1
}

_probe_backend() {
    [ -n "$_backend" ] && return 0
    local b i
    for b in $TB_ORDER; do
        case "$b" in
            adb) _probe_adb && { _backend=adb; return 0; } ;;
            rish)
                [ -x "$RISH" ] || continue
                for i in 1; do
                    timeout 12 "$RISH" -c 'echo ok' 2>/dev/null | grep -q '^ok' && { _backend=rish; return 0; }
                done ;;
            adb_cli)
                [ -x "$ADB_CLI" ] || continue
                timeout 15 "$ADB_CLI" local shell 'echo ok' 2>/dev/null | grep -q '^ok' && { _backend=adb_cli; return 0; } ;;
        esac
    done
    _backend=direct
}

backend_name() { _probe_backend; echo "$_backend"; }

privileged() {
    _probe_backend
    case "$_backend" in
        adb)     timeout 30 adb -s "$_serial" shell "$*" 2>/dev/null | tr -d '\r' ;;
        rish)    timeout 30 "$RISH" -c "$*" 2>/dev/null | sed '/^Waiting for Shizuku/d' ;;
        adb_cli) timeout 30 "$ADB_CLI" local shell "$*" 2>/dev/null ;;
        *)
            echo "[tool_backend] WARN: no privileged backend, running unprivileged: $*" >&2
            sh -c "$*" 2>/dev/null ;;
    esac
}

read_file() { cat "$1" 2>/dev/null || privileged "cat $1"; }

[ -n "${TB_NO_PROBE:-}" ] || _probe_backend
