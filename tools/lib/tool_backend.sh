#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../lib/miuiserperuser_common.sh"
RISH="$HOME/Rish/rish"
ADB_CMD="adb -s 127.0.0.1:5555 shell"
export RISH_APPLICATION_ID=com.termux
_backend=""

_probe_backend() {
    [ -n "$_backend" ] && return
    if [ -x "$RISH" ] && [ -f "$HOME/Rish/rish_shizuku.dex" ] && \
       RISH_APPLICATION_ID=com.termux "$RISH" -c "echo ok" 2>/dev/null | grep -q "^ok"; then
        _backend="rish"; return
    fi
    $ADB_CMD echo ok 2>/dev/null | grep -q "^ok" && _backend="adb" && return
    _backend="direct"
}

privileged() {
    _probe_backend
    case "$_backend" in
        rish)   RISH_APPLICATION_ID=com.termux "$RISH" -c "$*" 2>/dev/null ;;
        adb)    $ADB_CMD "$*" 2>/dev/null ;;
        direct) sh -c "$*" 2>/dev/null ;;
    esac
}

read_file() { cat "$1" 2>/dev/null || privileged "cat $1"; }
