#!/data/data/com.termux/files/usr/bin/bash
# court_dispatcher.sh — Court Dispatcher v2
# Part of MiuiserPeruser Judicial System v2
#
# CHANGES FROM v1:
#   - Emits WEIGHT (base signal weight) instead of SCORE (final score)
#   - Wire format: SOURCE|SIGNAL|WEIGHT|CONTEXT  (was SOURCE|SIGNAL|SCORE|CONTEXT)
#   - Weight table replaces old score table
#   - april_o_neil.sh now routes to scoring_engine.sh to compute final score
#
# This file remains a simple signal classifier — no scoring math lives here.
# All score computation is in scoring_engine.sh.
#
# Reads from: judgement.pipe  (written to by daemon fleet)
# Writes to:  april_o_neil.sh (via execution.pipe or direct call)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

PIPES_DIR="${BASE_DIR}/pipes"
JUDGEMENT_PIPE="${PIPES_DIR}/judgement.pipe"
EXECUTION_PIPE="${PIPES_DIR}/ingest.pipe"
LOG_FILE="${BASE_DIR}/logs/court_dispatcher.log"
IA_LOCK="${BASE_DIR}/state/internal_affairs.lock"

mkdir -p "$PIPES_DIR" "$(dirname "$LOG_FILE")"
[[ -p "$JUDGEMENT_PIPE" ]] || mkfifo "$JUDGEMENT_PIPE"

_log() {
    echo "[DISPATCHER] $(date +%s) $*" | tee -a "$LOG_FILE" >&2
}

# ── Base Signal Weight Table ───────────────────────────────────────────────────
# These are BASE weights only. scoring_engine.sh applies all modifiers on top:
#   user context, situational awareness, covariance, recidivism, source tier.

_signal_to_weight() {
    local signal="$1"
    case "$signal" in
        INTEGRITY_VIOLATION)   echo 50 ;;
        NETWORK_ANOMALY)       echo 35 ;;
        THERMAL_CRITICAL)      echo 40 ;;
        CPU_HOG_CRITICAL)      echo 30 ;;
        CPU_THROTTLING)        echo 20 ;;
        CPU_HOG)               echo 18 ;;
        THERMAL_WARN)          echo 15 ;;
        WAKELOCK_ANOMALY)      echo 10 ;;
        # Fallback for unknown signal types — conservative low weight
        *)                     echo 8  ;;
    esac
}

# ── Dispatch loop ─────────────────────────────────────────────────────────────
# Reads lines from judgement.pipe.
# Input format (from daemon fleet):  SOURCE|SIGNAL|CONTEXT
# Output format (to april_o_neil):   SOURCE|SIGNAL|WEIGHT|CONTEXT

_log "START listening on ${JUDGEMENT_PIPE}"

while IFS='|' read -r SOURCE SIGNAL CONTEXT; do
    [[ -z "$SOURCE" || -z "$SIGNAL" ]] && continue

    # Check for pipeline suspend
    if [[ -f "$IA_LOCK" ]]; then
        _log "SUSPENDED src=${SOURCE} signal=${SIGNAL} — pipeline locked by internal_affairs"
        continue
    fi

    WEIGHT=$(_signal_to_weight "$SIGNAL")

    _log "DISPATCH src=${SOURCE} signal=${SIGNAL} weight=${WEIGHT}"

    # Emit to execution pipe (april_o_neil reads this)
    # Wire format: SOURCE|SIGNAL|WEIGHT|CONTEXT
    echo "${SOURCE}|${SIGNAL}|${WEIGHT}|${CONTEXT:-}" >  "$EXECUTION_PIPE"

done < <(tail -f "$JUDGEMENT_PIPE")
