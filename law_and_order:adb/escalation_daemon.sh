#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
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
