#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../lib/miuiserperuser_common.sh"

# Baxter Stockman — Execution Arm
# Receives verdicts via execution.pipe, enforces via rish/adb
# FIX: Added THROTTLE and SHUTDOWN action cases

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
IN_PIPE="$BASE/pipes/execution.pipe"
LOG="$BASE/logs/baxter.log"
EVT="$BASE/state/court.events"
PIDDIR="$BASE/state/pids"

source "$LAW/court_registry_lib.sh" 2>/dev/null
source "$LAW/court_event_lib.sh" 2>/dev/null

mkdir -p "$BASE/logs"

log() {
    local ts
    ts=$(date "+%Y-%m-%d %H:%M:%S")
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

# Kill a native daemon by PID file (for THROTTLE on syndicate daemons)
kill_native() {
    local name="$1"
    local pidfile="$PIDDIR/${name}.pid"
    if [ -f "$pidfile" ] && kill -0 "$(cat "$pidfile")" 2>/dev/null; then
        kill "$(cat "$pidfile")" 2>/dev/null
        log "NATIVE_KILL: $name (PID $(cat "$pidfile")) — sentencing engine will restart"
    else
        log "NATIVE_KILL: no valid PID for $name"
    fi
}

declare -A SEEN
MAX_SEEN=500

log "ONLINE — Active Intervention Mode"

[ -p "$IN_PIPE" ] || mkfifo "$IN_PIPE"
exec 3<>"$IN_PIPE"

while IFS="|" read -r action target ctx <&3; do
    [[ -z "$action" ]] && continue

    KEY="$action|$target"

    if [[ "${SEEN[$KEY]}" == "1" ]]; then
        log "SKIP (already handled): $KEY"
        continue
    fi

    if [[ ${#SEEN[@]} -gt $MAX_SEEN ]]; then
        unset SEEN; declare -A SEEN
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
        THROTTLE)
            # FIX: implemented
            # all_non_critical → am kill-all (kills cached background processes, safe)
            # specific daemon name → kill by pidfile, sentencing engine restarts it
            log "THROTTLE $target"
            if [ "$target" = "all_non_critical" ]; then
                run_shell "am kill-all"
                log "THROTTLE: killed all cached background processes"
            else
                # Native syndicate daemon — interrupt current poll cycle
                kill_native "$target"
                # Also try am kill in case it's an Android process
                run_shell "am kill $target" 2>/dev/null
            fi
            (flock -x 200; echo "$(date +%s)|BAXTER|THROTTLE|$target:$ctx" >> "$EVT") 200>"$EVT.lock"
            ;;
        SHUTDOWN)
            # FIX: implemented
            # non_essential → kill-all cached + force-stop known heavy apps
            log "SHUTDOWN $target"
            if [ "$target" = "non_essential" ]; then
                run_shell "am kill-all"
                # Force-stop common resource hogs — extend list as needed
                for pkg in \
                    com.google.android.youtube \
                    com.facebook.katana \
                    com.instagram.android \
                    com.tiktok.android \
                    com.twitter.android \
                    com.snapchat.android; do
                    run_shell "am force-stop $pkg" 2>/dev/null
                done
                log "SHUTDOWN: killed non-essential apps"
            else
                run_shell "am force-stop $target"
            fi
            (flock -x 200; echo "$(date +%s)|BAXTER|SHUTDOWN|$target:$ctx" >> "$EVT") 200>"$EVT.lock"
            ;;
        ESCALATE)
            log "ESCALATION_FORWARD $target"
            [ -p "$BASE/pipes/escalation.pipe" ] && \
                ( echo "$action|$target|$ctx" > "$BASE/pipes/escalation.pipe" ) & disown $!
            ;;
        *)
            log "UNHANDLED: $action $target"
            ;;
    esac

done
