#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}


BASE="$BASE"
LAW="$BASE/law_and_order:adb"
IN_PIPE="$BASE/pipes/escalation.pipe"
LOG="$BASE/logs/escalation.log"
EVT="$BASE/state/court.events"

source "$LAW/court_event_lib.sh"

mkdir -p "$BASE/logs"

log() {
    echo "[ESCALATION] $(date +%s) $1" >> "$LOG"
}

log "ONLINE"

[ -p "$IN_PIPE" ] || mkfifo "$IN_PIPE"

exec 3<>"$IN_PIPE"  # keep open — prevents EOF busy loop

while IFS="|" read -r action target ctx <&3; do
    [ -z "$action" ] && continue

    log "ESCALATE $action | $target | $ctx"

    emit_event "ESCALATION" "$action" "$target:$ctx"

    # Tiered handoff
    case "$action" in
        JAIL)
            bash "$LAW/jailhouse_manager.sh" jail "$target" "$ctx" 2>/dev/null
            ;;
        QUARANTINE)
            bash "$BASE/src/core/turtlepower_engine.sh" 2>/dev/null
            ;;
        NOTIFY)
            log "NOTIFY: $target — $ctx"
            ;;
        *)
            log "UNKNOWN ACTION: $action"
            ;;
    esac

done
