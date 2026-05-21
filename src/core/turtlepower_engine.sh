#!/data/data/com.termux/files/usr/bin/bash
# ============================================================
# turtlepower_engine.sh — Rule evaluator (one-shot, called by daemon)
# ============================================================
# UPDATE: value-based rule evaluation reads live metrics from
#         turtlecomd.log — no longer count-only for thermal/cpu rules.
#
# Condition dispatch:
#   thermal_peak, thermal_zones, cpu_pct, warn_zones → VALUE mode
#   everything else                                   → COUNT mode

BASE="$HOME/MiuiserPeruser"
EVENT_FILE="$BASE/state/court.events"
RULE_FILE="$BASE/state/turtlepower.rules"
QUAR_FILE="$BASE/state/quarantine.state"
REG="$BASE/state/court.registry"
LOCK="$BASE/state/turtlepower.lock"
PIDDIR="$BASE/state/pids"
EXEC_PIPE="$BASE/pipes/execution.pipe"
TCOMD_LOG="$BASE/logs/turtlecomd.log"
# Auto-rotate log if > 10MB
MAX_LOG_BYTES=10485760  # 10MB
if [ -f "$TCOMD_LOG" ] && [ "$(stat -c%s "$TCOMD_LOG" 2>/dev/null)" -gt "$MAX_LOG_BYTES" ]; then
    mv "$TCOMD_LOG" "${TCOMD_LOG}.old"
    echo "[TP_ENGINE] Log rotated at $(date)" > "$TCOMD_LOG"
fi

mkdir -p "$(dirname "$QUAR_FILE")"
touch "$QUAR_FILE"

# -------------------------
# LOCK FLAGS
# -------------------------
read_lock_val() {
    grep "^$1=" "$LOCK" 2>/dev/null | cut -d'=' -f2
}

allow_daemons=$(read_lock_val ALLOW_DAEMONS)
lock_state=$(read_lock_val LOCK_STATE)

if [ "$lock_state" != "ACTIVE" ]; then
    echo "[TP_ENGINE] LOCK_STATE=$lock_state — skipping"
    exit 0
fi

# -------------------------
# VALUE-BASED METRIC READER
# Reads latest value for a named metric from turtlecomd.log
# -------------------------
# Known metric keys (anything else → count mode)
METRIC_KEYS="thermal_peak thermal_zones cpu_pct warn_zones"

is_metric_key() {
    [[ " $METRIC_KEYS " == *" $1 "* ]]
}

read_metric_value() {
    local metric="$1"
    local val=""

    case "$metric" in
        thermal_peak)
            val=$(grep "leatherheadd|thermal_critical" "$TCOMD_LOG" 2>/dev/null | \
                  tail -n 1 | grep -o 'peak_c=[0-9.]*' | cut -d= -f2)
            ;;
        thermal_zones)
            val=$(grep "leatherheadd|thermal_critical" "$TCOMD_LOG" 2>/dev/null | \
                  tail -n 1 | grep -o 'critical_zones=[0-9]*' | cut -d= -f2)
            ;;
        warn_zones)
            val=$(grep "leatherheadd|thermal_critical" "$TCOMD_LOG" 2>/dev/null | \
                  tail -n 1 | grep -o 'warn_zones=[0-9]*' | cut -d= -f2)
            ;;
        cpu_pct)
            val=$(grep "ratkingd|cpu_hog" "$TCOMD_LOG" 2>/dev/null | \
                  tail -n 1 | grep -o 'cpu_pct=[0-9]*' | cut -d= -f2)
            ;;
    esac

    # Return integer part for comparison (strips decimal)
    echo "${val%.*}"
}

# -------------------------
# EXECUTION PIPE HELPER
# -------------------------
send_exec() {
    local action="$1" target="$2" ctx="$3"
    if [ -p "$EXEC_PIPE" ]; then
        ( echo "$action|$target|$ctx" > "$EXEC_PIPE" ) &
        disown $!
    else
        echo "[TP_ENGINE] WARNING: execution.pipe missing — $action $target dropped" >&2
    fi
}

# -------------------------
# QUARANTINE / RESTART HELPERS
# -------------------------
is_quarantined() { grep -q "^$1|" "$QUAR_FILE" 2>/dev/null; }

quarantine() {
    local name="$1" reason="${2:-rule}"
    (
        flock -x 200
        grep -v "^$name|" "$QUAR_FILE" > "$QUAR_FILE.tmp" 2>/dev/null
        mv "$QUAR_FILE.tmp" "$QUAR_FILE"
        echo "$name|$reason|$(date +%s)" >> "$QUAR_FILE"
    ) 200>"$QUAR_FILE.lock"
    local pidfile="$PIDDIR/${name}.pid"
    [ -f "$pidfile" ] && kill "$(cat "$pidfile")" 2>/dev/null
}

release() {
    local name="$1"
    (
        flock -x 200
        grep -v "^$name|" "$QUAR_FILE" > "$QUAR_FILE.tmp" 2>/dev/null
        mv "$QUAR_FILE.tmp" "$QUAR_FILE"
    ) 200>"$QUAR_FILE.lock"
}

restart_daemon() {
    local name="$1"
    [ "$allow_daemons" != "1" ] && return
    is_quarantined "$name" && return 0

    local LAW="$BASE/law_and_order:adb"
    declare -A DAEMON_SCRIPTS=(
        [court_core_engine]="$LAW/court_core_engine.sh"
        [court_orchestrator]="$LAW/court_orchestrator.sh"
        [april_o_neil]="$LAW/cre/april_o_neil.sh"
        [escalation]="$LAW/escalation_daemon.sh"
        [visitors_pass]="$LAW/visitors_pass_daemon.sh"
        [turtlepower]="$LAW/turtlepower_daemon.sh"
    )

    local pidfile="$PIDDIR/${name}.pid"
    [ -f "$pidfile" ] && kill "$(cat "$pidfile")" 2>/dev/null && rm -f "$pidfile"

    local script="${DAEMON_SCRIPTS[$name]}"
    [ -z "$script" ] && echo "[TP_ENGINE] WARNING: no script for '$name'" >&2 && return 1

    nohup bash "$script" >> "$BASE/logs/judicial_controller.log" 2>&1 &
    echo "$!" > "$pidfile"
    echo "[TP_ENGINE] Restarted $name (PID $!)"
}

restart_any_stopped() {
    [ "$allow_daemons" != "1" ] && return
    while IFS='|' read -r name state pid; do
        [ "$name" = "# NAME" ] || [ -z "$name" ] && continue
        [ "$state" = "STOPPED" ] && restart_daemon "$name"
    done < "$REG"
}

# -------------------------
# ACTION DISPATCHER
# -------------------------
apply() {
    local cond="$1" action="$2"
    case "$action" in
        quarantine:*) quarantine "${action#quarantine:}" "$cond" ;;
        release:*)    release "${action#release:}" ;;
        restart:any)  restart_any_stopped ;;
        restart:*)    restart_daemon "${action#restart:}" ;;
        throttle:*)
            local target="${action#throttle:}"
            echo "[TP_ENGINE] THROTTLE → $target"
            send_exec "THROTTLE" "$target" "rule=$cond"
            (flock -x 200; echo "$(date +%s)|TURTLEPOWER|THROTTLE|$target:$cond" >> "$EVENT_FILE") \
                200>"$EVENT_FILE.lock"
            ;;
        shutdown:*)
            local target="${action#shutdown:}"
            echo "[TP_ENGINE] SHUTDOWN → $target"
            send_exec "SHUTDOWN" "$target" "rule=$cond"
            (flock -x 200; echo "$(date +%s)|TURTLEPOWER|SHUTDOWN|$target:$cond" >> "$EVENT_FILE") \
                200>"$EVENT_FILE.lock"
            ;;
        *) echo "[TP_ENGINE] WARNING: unknown action '$action'" >&2 ;;
    esac
}

# -------------------------
# COUNT MODE
# -------------------------
count_matching_events() {
    local key="$1" count=0
    if [[ "$key" == *":"* ]]; then
        local src="${key%%:*}" evt="${key##*:}"
        count=$(grep -c "^[^|]*|${src}|${evt}|" "$EVENT_FILE" 2>/dev/null || echo 0)
    else
        count=$(grep -c "^[^|]*|[^|]*|${key}|" "$EVENT_FILE" 2>/dev/null || echo 0)
        [ "$count" -eq 0 ] && \
            count=$(grep -c "^[^|]*|[^|]*|[^|]*|.*${key}.*" "$EVENT_FILE" 2>/dev/null || echo 0)
    fi
    echo "$count"
}

# -------------------------
# RULE EVALUATION LOOP
# -------------------------
echo "[TP_ENGINE] Evaluating rules at $(date)"

while IFS='|' read -r prefix cond action; do
    [ "$prefix" != "RULE" ] && continue
    [ -z "$cond" ] || [ -z "$action" ] && continue

    key="${cond%%>=*}"
    num="${cond##*>=}"

    [[ "$num" =~ ^[0-9]+$ ]] || {
        echo "[TP_ENGINE] WARNING: bad condition '$cond'" >&2; continue
    }

    if is_metric_key "$key"; then
        # VALUE MODE — read live metric from turtlecomd.log
        val=$(read_metric_value "$key")
        if [ -z "$val" ]; then
            echo "[TP_ENGINE] METRIC: $key — no data yet, skipping"
            continue
        fi
        echo "[TP_ENGINE] METRIC: $key=$val (threshold $num)"
        if [ "$val" -ge "$num" ] 2>/dev/null; then
            echo "[TP_ENGINE] RULE TRIGGERED: $cond ($val >= $num) → $action"
            apply "$cond" "$action"
        fi
    else
        # COUNT MODE — count events in court.events
        count=$(count_matching_events "$key")
        if [ "$count" -ge "$num" ]; then
            echo "[TP_ENGINE] RULE TRIGGERED: $cond ($count >= $num) → $action"
            apply "$cond" "$action"
        fi
    fi

done < "$RULE_FILE"

echo "[TP_ENGINE] Evaluation complete"
