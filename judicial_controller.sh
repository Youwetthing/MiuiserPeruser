#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
REG="$BASE/state/court.registry"
LOG="$BASE/logs/judicial_controller.log"

mkdir -p "$BASE/logs"

court_status() {
    case "$1" in
        online) echo "⚖️ Court is in session" ;;
        offline) echo "🧾 Your case is dismissed" ;;
        starting) echo "⚖️ Court is convening" ;;
        stopping) echo "⚖️ Court is adjourned" ;;
    esac
}

status_system() {
    echo "=========================="
    court_status online
    echo "=========================="
    echo ""
    echo "📜 COURT REGISTRY:"
    cat "$REG"
}

case "$1" in
    status)
        status_system
        ;;
    *)
        echo "Usage: status"
        ;;
esac
