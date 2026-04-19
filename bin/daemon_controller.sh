#!/data/data/com.termux/files/usr/bin/bash

REGISTRY="$HOME/MiuiserPeruser/Registry/daemon_allowlist.json"
BIN_DIR="$HOME/MiuiserPeruser/bin"
LOG_DIR="$HOME/MiuiserPeruser/Log_Cabin"

mkdir -p "$LOG_DIR"

start_daemon() {
    local d="$1"
    if pgrep -f "$d" > /dev/null; then
        echo "[=] $d already running"
    else
        echo "[+] Starting $d"
        nohup "$BIN_DIR/$d" >> "$LOG_DIR/$d.log" 2>&1 &
    fi
}

stop_daemon() {
    local d="$1"
    echo "[-] Stopping $d"
    pkill -f "$d" 2>/dev/null || true
}

status_daemon() {
    local d="$1"
    if pgrep -f "$d" > /dev/null; then
        echo "[RUNNING] $d"
    else
        echo "[STOPPED] $d"
    fi
}

run_group() {
    local group="$1"
    jq -r ".${group}[]" "$REGISTRY" | while read -r d; do
        start_daemon "$d"
    done
}

stop_group() {
    local group="$1"
    jq -r ".${group}[]" "$REGISTRY" | while read -r d; do
        stop_daemon "$d"
    done
}

status_group() {
    local group="$1"
    jq -r ".${group}[]" "$REGISTRY" | while read -r d; do
        status_daemon "$d"
    done
}

case "$1" in
    start)
        run_group "$2"
        ;;
    stop)
        stop_group "$2"
        ;;
    status)
        status_group "$2"
        ;;
    *)
        echo "Usage:"
        echo "  daemon_controller.sh start <group>"
        echo "  daemon_controller.sh stop <group>"
        echo "  daemon_controller.sh status <group>"
        ;;
esac
