#!/data/data/com.termux/files/usr/bin/bash

# =========================
# JUDGE_EXECUTOR
# Judgement pipe consumer
# =========================
# FIX: QUARANTINED branch was calling turtlepower_engine.sh (full rule engine)
#      Replaced with direct quarantine state write via sourced jailhouse_manager
# FIX: jailhouse_manager sourced directly — functions called inline,
#      no CLI dispatch needed, no subprocess overhead

BASE="$HOME/MiuiserPeruser"
LAW="$BASE/law_and_order:adb"
PIPE="$BASE/pipes/judgement.pipe"
CASE_DIR="$BASE/cre/cases"
LOG="$BASE/cre/judge_executor.log"
EVT="$BASE/state/court.events"
QUAR_FILE="$BASE/state/quarantine.state"
PIDDIR="$BASE/state/pids"

source "$LAW/court_registry_lib.sh"
source "$LAW/jailhouse_manager.sh"          # FIX: source for direct function calls
source "$LAW/state_tracker.sh" 2>/dev/null || true

EXEC_PIPE="$BASE/pipes/execution.pipe"

enforce() {
    local action="$1" target="$2" ctx="$3"
    # Non-blocking write — Baxter may be slow
    [ -p "$EXEC_PIPE" ] && ( echo "$action|$target|$ctx" > "$EXEC_PIPE" ) & disown $!
}

mkdir -p "$BASE/cre"

log() { echo "[JUDGE_EXECUTOR] $(date +%s) $1" | tee -a "$LOG"; }

[ -p "$PIPE" ] || mkfifo "$PIPE"

log "ONLINE — awaiting judgement"

exec 3<>"$PIPE"

while IFS='|' read -r src sig score ctx <&3; do
    [ -z "$src" ] && continue

    log "Judging: src=$src sig=$sig score=$score"

    case_file=$(grep -rl "\"source\": \"$src\"" "$CASE_DIR" 2>/dev/null | \
                xargs grep -l "PENDING_JUDGEMENT" 2>/dev/null | tail -n 1)

    if [ -z "$case_file" ]; then
        log "WARNING: no pending case file found for $src"
        continue
    fi

    if [ "$score" -ge 80 ]; then
        verdict="JAILED"
        log "VERDICT: $src → JAILED (score $score)"
        jail "$src" "score=$score"              # FIX: direct function call
        bash "$LAW/write_criminal_record.sh" "$src" "JAILED" "score=$score" "judge_executor"
        update_state "$src" "JAILED" 2>/dev/null
        enforce "KILL" "$src" "score=$score"
        (flock -x 200; echo "$(date +%s)|JUDGE|JAILED|$src:score=$score" >> "$EVT") 200>"$EVT.lock"

    elif [ "$score" -ge 50 ]; then
        verdict="QUARANTINED"
        log "VERDICT: $src → QUARANTINED (score $score)"

        # FIX: was calling turtlepower_engine.sh — replaced with direct quarantine write
        (
            flock -x 200
            grep -v "^$src|" "$QUAR_FILE" > "$QUAR_FILE.tmp" 2>/dev/null
            mv "$QUAR_FILE.tmp" "$QUAR_FILE"
            echo "$src|rule|$(date +%s)" >> "$QUAR_FILE"
        ) 200>"$QUAR_FILE.lock"

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

    sed -i "s/PENDING_JUDGEMENT/$verdict/" "$case_file"

done
