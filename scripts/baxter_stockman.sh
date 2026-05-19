#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# Baxter Stockman — Execution Arm
# Receives verdicts via execution.pipe, enforces via rish/adb

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
IN_PIPE="$BASE/pipes/execution.pipe"
LOG="$BASE/logs/baxter.log"
EVT="$BASE/state/court.events"

source "$LAW/court_registry_lib.sh" 2>/dev/null
source "$LAW/court_event_lib.sh" 2>/dev/null

mkdir -p "$BASE/logs"

log() {
    local ts=$(date "+%Y-%m-%d %H:%M:%S")
    echo "[$ts] [BAXTER] $1" >> "$LOG"
}

run_shell() {
    if [[ -x "$BASE/rish" ]]; then
        "$BASE/rish" -c "$*" 2>/dev/null
    elif command -v adb >/dev/null 2>&1; then
        adb shell "$@" 2>/dev/null
    else
        log "ERROR: no rish or adb available"
        return 1
    fi
}

declare -A SEEN
MAX_SEEN=500

log "ONLINE — Active Intervention Mode"

[ -p "$IN_PIPE" ] || mkfifo "$IN_PIPE"

exec 3<>"$IN_PIPE"  # keep pipe open — prevents EOF busy loop

while IFS="|" read -r action target ctx <&3; do
    [[ -z "$action" ]] && continue

    KEY="$action|$target"

    # Idempotency guard
    if [[ "${SEEN[$KEY]}" == "1" ]]; then
        log "SKIP (already handled): $KEY"
        continue
    fi

    # GC seen map
    if [[ ${#SEEN[@]} -gt $MAX_SEEN ]]; then
        unset SEEN
        declare -A SEEN
    fi
    SEEN[$KEY]=1

    log "ACTION: $action | TARGET: $target | CTX: $ctx"

    case "$action" in
        KILL)
            log "FORCE_STOP $target"
            run_shell "am force-stop $target"
            (flock -x 200; echo "$(date +%s)|BAXTER|KILL|$target:$ctx" >> "$EVT") 200>"$EVT.lock"
            ;;
        ISOLATE)
            log "ISOLATE $target"
            run_shell "pm disable-user --user 0 $target"
            (flock -x 200; echo "$(date +%s)|BAXTER|ISOLATE|$target:$ctx" >> "$EVT") 200>"$EVT.lock"
            ;;
        INTERVENE)
            log "SOFT_KILL $target"
            run_shell "am kill $target"
            (flock -x 200; echo "$(date +%s)|BAXTER|INTERVENE|$target:$ctx" >> "$EVT") 200>"$EVT.lock"
            ;;
        ESCALATE)
            log "ESCALATION_FORWARD $target"
            [ -p "$BASE/pipes/escalation.pipe" ] &&                 echo "$action|$target|$ctx" > "$BASE/pipes/escalation.pipe"
            ;;
        *)
            log "UNHANDLED: $action $target"
            ;;
    esac

done
