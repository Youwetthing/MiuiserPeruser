#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../lib/miuiserperuser_common.sh"

# ============================================================
# SUPERHERO_ADAPTER
# execution.pipe consumer
# Bridges judicial verdicts → privileged execution via rish/adb
#
# Reads:  pipes/execution.pipe  (ACTION|TARGET|CTX)
# Writes: cre/ledger.log        (append, flock)
#         state/court.events    (append, flock)
#         cre/superhero_adapter.log
#
# Actions:
#   KILL      score≥80  native daemon → kill by PID file
#                       app           → am force-stop
#   ISOLATE   score≥50  native daemon → kill by PID file
#                       app           → pm disable-user --user 0
#   INTERVENE score<50  native daemon → SIGTERM only (no escalation)
#                       app           → am kill
#
# Note: native daemon = has a PID file in state/pids/
# ============================================================

BASE="$BASE"
LAW="$BASE/law_and_order:adb"
PIPE="$BASE/pipes/execution.pipe"
LOG="$BASE/cre/superhero_adapter.log"
LEDGER="$BASE/cre/ledger.log"
EVT="$BASE/state/court.events"
PIDDIR="$BASE/state/pids"

RISH="/data/data/com.termux/files/home/.shizuku/rish"
RISH_TIMEOUT=60
source "$LAW/court_registry_lib.sh"

# ── Logging ────────────────────────────────────────────────

log() {
    echo "[SUPERHERO_ADAPTER] $(date +%s) $*" | tee -a "$LOG"
}

ledger_append() {
    local action="$1" target="$2" result="$3"
    (flock -x 200
     echo "$(date +%s)|SUPERHERO_ADAPTER|$action|$target|$result" >> "$LEDGER"
    ) 200>"$LEDGER.lock"
}

evt_append() {
    local tag="$1" detail="$2"
    (flock -x 200
     echo "$(date +%s)|SUPERHERO|$tag|$detail" >> "$EVT"
    ) 200>"$EVT.lock"
}

# ── Privileged execution ────────────────────────────────────
# rish with 60s timeout; rc=124 → soft skip (timeout, not failure)
# Falls back to adb shell if rish not available.

priv_exec() {
    local cmd="$1"
    if [ -x "$RISH" ]; then
        timeout "$RISH_TIMEOUT" "$RISH" -c "$cmd" 2>/dev/null
        local rc=$?
        if [ "$rc" -eq 124 ]; then
            log "WARN: rish timeout ($RISH_TIMEOUT s) on: $cmd — soft skip"
            return 124
        fi
        return "$rc"
    fi
    # rish not available — try adb
    if command -v adb >/dev/null 2>&1; then
        log "WARN: rish unavailable — falling back to adb shell"
        adb shell "$cmd" 2>/dev/null
        return $?
    fi
    log "ERROR: no privileged backend available (rish/adb both missing)"
    return 1
}

# ── Target classification ───────────────────────────────────
# A target is a "native daemon" if it has a PID file in state/pids/.
# This covers all MiuiserPeruser daemons regardless of naming convention.

is_native_daemon() {
    local target="$1"
    [ -f "$PIDDIR/${target}.pid" ]
}

# ── PID-file kill ───────────────────────────────────────────
# Sends SIGTERM; escalates to SIGKILL after 1 s if still alive.
# Removes stale PID file on success.

kill_by_pid_file() {
    local target="$1"
    local pid_file="$PIDDIR/${target}.pid"

    if [ ! -f "$pid_file" ]; then
        log "WARN: no PID file for $target at $pid_file"
        return 1
    fi

    local pid
    pid=$(cat "$pid_file" 2>/dev/null)

    if [ -z "$pid" ]; then
        log "WARN: empty PID file for $target"
        rm -f "$pid_file"
        return 1
    fi

    if ! kill -0 "$pid" 2>/dev/null; then
        log "WARN: stale PID file for $target (pid=$pid not running)"
        rm -f "$pid_file"
        return 1
    fi

    kill -TERM "$pid" 2>/dev/null
    sleep 1

    if kill -0 "$pid" 2>/dev/null; then
        log "ESCALATE: $target pid=$pid did not exit on SIGTERM — sending SIGKILL"
        kill -KILL "$pid" 2>/dev/null
    fi

    rm -f "$pid_file"
    log "KILL_PID: $target pid=$pid terminated"
    return 0
}

# ── Action handlers ─────────────────────────────────────────

do_kill() {
    local target="$1" ctx="$2"
    log "ACTION=KILL target=$target ctx=$ctx"

    if is_native_daemon "$target"; then
        kill_by_pid_file "$target"
        local rc=$?
        ledger_append "KILL" "$target" "method=pid_kill,rc=$rc"
        evt_append "KILL_EXECUTED" "$target:method=pid_kill,rc=$rc"
    else
        priv_exec "am force-stop $target"
        local rc=$?
        [ "$rc" -eq 124 ] && log "SKIP: rish timeout on force-stop $target"
        ledger_append "KILL" "$target" "method=force_stop,rc=$rc"
        evt_append "KILL_EXECUTED" "$target:method=force_stop,rc=$rc"
    fi
}

do_isolate() {
    local target="$1" ctx="$2"
    log "ACTION=ISOLATE target=$target ctx=$ctx"

    if is_native_daemon "$target"; then
        # Native daemons: kill by PID file (not pm disable-user)
        kill_by_pid_file "$target"
        local rc=$?
        ledger_append "ISOLATE" "$target" "method=pid_kill,rc=$rc"
        evt_append "ISOLATE_EXECUTED" "$target:method=pid_kill,rc=$rc"
    else
        priv_exec "pm disable-user --user 0 $target"
        local rc=$?
        [ "$rc" -eq 124 ] && log "SKIP: rish timeout on pm disable-user $target"
        ledger_append "ISOLATE" "$target" "method=pm_disable,rc=$rc"
        evt_append "ISOLATE_EXECUTED" "$target:method=pm_disable,rc=$rc"
    fi
}

do_intervene() {
    local target="$1" ctx="$2"
    log "ACTION=INTERVENE target=$target ctx=$ctx"

    if is_native_daemon "$target"; then
        # Soft touch — SIGTERM only, no SIGKILL escalation
        local pid_file="$PIDDIR/${target}.pid"
        if [ -f "$pid_file" ]; then
            local pid
            pid=$(cat "$pid_file" 2>/dev/null)
            if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
                kill -TERM "$pid" 2>/dev/null
                log "INTERVENE_SIGTERM: $target pid=$pid"
            else
                log "WARN: $target not running (pid=$pid)"
            fi
        else
            log "WARN: no PID file for intervene target $target"
        fi
        ledger_append "INTERVENE" "$target" "method=sigterm"
        evt_append "INTERVENE_EXECUTED" "$target:method=sigterm"
    else
        priv_exec "am kill $target"
        local rc=$?
        [ "$rc" -eq 124 ] && log "SKIP: rish timeout on am kill $target"
        ledger_append "INTERVENE" "$target" "method=am_kill,rc=$rc"
        evt_append "INTERVENE_EXECUTED" "$target:method=am_kill,rc=$rc"
    fi
}

# ── Main loop ───────────────────────────────────────────────

mkdir -p "$BASE/cre" "$PIDDIR"
[ -p "$PIPE" ] || mkfifo "$PIPE"

log "ONLINE pid=$$ — awaiting execution orders on $PIPE"

exec 3<>"$PIPE"

while IFS='|' read -r action target ctx <&3; do
    [ -z "$action" ] && continue
    log "ORDER: action=$action target=$target ctx=$ctx"

    case "$action" in
        KILL)      do_kill      "$target" "$ctx" ;;
        ISOLATE)   do_isolate   "$target" "$ctx" ;;
        INTERVENE) do_intervene "$target" "$ctx" ;;
        *)         log "UNKNOWN action: $action (target=$target) — ignored" ;;
    esac
    rotate_court_events
done

log "OFFLINE — pipe closed"
