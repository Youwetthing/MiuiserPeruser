#!/data/data/com.termux/files/usr/bin/bash

# =========================
# JUDGE_EXECUTOR
# Judgement pipe consumer
# =========================

BASE="$HOME/MiuiserPeruser"
LAW="$BASE/law_and_order:adb"
PIPE="$BASE/pipes/judgement.pipe"
CASE_DIR="$BASE/cre/cases"
LOG="$BASE/cre/judge_executor.log"
EVT="$BASE/state/court.events"

source "$LAW/court_registry_lib.sh"
source "$LAW/state_tracker.sh" 2>/dev/null || true

EXEC_PIPE="$BASE/pipes/execution.pipe"

enforce() {
    local action="$1" target="$2" ctx="$3"
    [ -p "$EXEC_PIPE" ] && echo "$action|$target|$ctx" > "$EXEC_PIPE"
}

mkdir -p "$BASE/cre"

log() { echo "[JUDGE_EXECUTOR] $(date +%s) $1" | tee -a "$LOG"; }

[ -p "$PIPE" ] || mkfifo "$PIPE"

log "ONLINE — awaiting judgement"

exec 3<>"$PIPE"  # keep pipe open, prevents busy loop on EOF

while IFS='|' read -r src sig score ctx <&3; do
    [ -z "$src" ] && continue

    log "Judging: src=$src sig=$sig score=$score"

    # Find matching case file
    case_file=$(grep -rl "\"source\": \"$src\"" "$CASE_DIR" 2>/dev/null | \
                xargs grep -l "PENDING_JUDGEMENT" 2>/dev/null | tail -n 1)

    if [ -z "$case_file" ]; then
        log "WARNING: no pending case file found for $src"
        continue
    fi

    # Enforce based on score
    if [ "$score" -ge 80 ]; then
        verdict="JAILED"
        log "VERDICT: $src → JAILED (score $score)"
        bash "$LAW/jailhouse_manager.sh" jail "$src" "score=$score" 2>/dev/null
        bash "$LAW/write_criminal_record.sh" "$src" "JAILED" "score=$score" "judge_executor"
        update_state "$src" "JAILED" 2>/dev/null
        enforce "KILL" "$src" "score=$score"
        (flock -x 200; echo "$(date +%s)|JUDGE|JAILED|$src:score=$score" >> "$EVT") 200>"$EVT.lock"
    elif [ "$score" -ge 50 ]; then
        verdict="QUARANTINED"
        log "VERDICT: $src → QUARANTINED (score $score)"
        bash "$BASE/src/core/turtlepower_engine.sh" 2>/dev/null
        bash "$LAW/write_criminal_record.sh" "$src" "QUARANTINED" "score=$score" "judge_executor"
        update_state "$src" "QUARANTINED" 2>/dev/null
        enforce "ISOLATE" "$src" "score=$score"
        (flock -x 200; echo "$(date +%s)|JUDGE|QUARANTINED|$src:score=$score" >> "$EVT") 200>"$EVT.lock"
    else
        verdict="DISMISSED"
        log "VERDICT: $src → DISMISSED (score $score)"
        bash "$LAW/write_criminal_record.sh" "$src" "DISMISSED" "score=$score" "judge_executor"
        update_state "$src" "DISMISSED" 2>/dev/null
        enforce "INTERVENE" "$src" "score=$score"
        (flock -x 200; echo "$(date +%s)|JUDGE|DISMISSED|$src:score=$score" >> "$EVT") 200>"$EVT.lock"
    fi

    # Update case status
    sed -i "s/PENDING_JUDGEMENT/$verdict/" "$case_file"

done
