#!/data/data/com.termux/files/usr/bin/bash
# april_o_neil.sh — Case Reporter & Evidence Enrichment v2
# Part of MiuiserPeruser Judicial System v2
#
# CHANGES FROM v1:
#   - Reads SOURCE|SIGNAL|WEIGHT|CONTEXT (new wire format)
#   - Routes each signal through scoring_engine.sh before case assembly
#   - Attaches full evidence bundle to case JSON (score, state, reasoning ref)
#   - Case JSON now includes: score, verdict_state, score_delta, evidence_bundle
#   - Passes enriched case to judge_executor.sh
#
# Reads from:  execution.pipe
# Calls:       scoring_engine.sh
# Writes:      cre/cases/case_<id>.json
# Pipes to:    judge_executor.sh (via judgement dispatch)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

LAW_DIR="${BASE_DIR}/law_and_order:adb"
PIPES_DIR="${BASE_DIR}/pipes"
CRE_DIR="${BASE_DIR}/cre"
STATE_DIR="${BASE_DIR}/state"

EXECUTION_PIPE="${PIPES_DIR}/ingest.pipe"
APRIL_LOG="${CRE_DIR}/april.log"
SCORING_ENGINE="${LAW_DIR}/scoring_engine.sh"
JUDGE_PIPE="${PIPES_DIR}/judgement.pipe"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"

mkdir -p "$CRE_DIR/cases" "$(dirname "$APRIL_LOG")"
touch "$APRIL_LOG"

_log() {
    echo "[APRIL] $(date +%s) $*" >> "$APRIL_LOG"
}

# ── Evidence bundle builder ────────────────────────────────────────────────────
# Assembles JSON evidence bundle attached to each case.
# Contains: score result, daemon_source, signal history from window.

_build_evidence_bundle() {
    local source="$1"
    local signal="$2"
    local weight="$3"
    local score="$4"
    local verdict_state="$5"
    local score_delta="$6"
    local case_epoch="$7"
    local context="$8"
    local daemon_source="$9"

    # Pull recent signals from window for this source (last 120s)
    local window_signals="[]"
    if [[ -f "${STATE_DIR}/signal_window.state" ]]; then
        local window_cutoff=$(( case_epoch - 120 ))
        window_signals=$(awk -F'|' \
            -v src="$source" -v cutoff="$window_cutoff" \
            'BEGIN { printf "[" }
            $1==src && $3>cutoff {
                if (NR>1) printf ","
                printf "{\"signal\":\"%s\",\"epoch\":%s}", $2, $3
            }
            END { printf "]" }' \
            "${STATE_DIR}/signal_window.state" 2>/dev/null || echo "[]")
    fi

    # Pull last scoring reasoning line reference
    local reasoning_ref=""
    if [[ -f "${CRE_DIR}/scoring_reasoning.log" ]]; then
        reasoning_ref=$(grep "src=${source} signal=${signal}" \
            "${CRE_DIR}/scoring_reasoning.log" | tail -1 || true)
    fi

    cat <<JSON
{
  "score": ${score},
  "score_delta": ${score_delta},
  "verdict_state": "${verdict_state}",
  "base_weight": ${weight},
  "daemon_source": "${daemon_source}",
  "signal_window": ${window_signals},
  "reasoning_ref": $(echo "$reasoning_ref" | python3 -c 'import sys,json; print(json.dumps(sys.stdin.read().strip()))' 2>/dev/null || echo '""'),
  "context": $(echo "$context" | python3 -c 'import sys,json; print(json.dumps(sys.stdin.read().strip()))' 2>/dev/null || echo '""')
}
JSON
}

# ── Case assembler ─────────────────────────────────────────────────────────────

_assemble_case() {
    local source="$1"
    local signal="$2"
    local weight="$3"
    local context="${4:-}"
    local NOW
    NOW=$(date +%s%3N)   # millisecond epoch for unique case IDs
    local EPOCH_S=$(( NOW / 1000 ))

    # Extract daemon_source from context field if present (format: daemon=<name>;...)
    local daemon_source
    daemon_source=$(echo "$context" | grep -oP '(?<=daemon=)[^;]+' || echo "unknown")

    # ── Route through scoring engine ─────────────────────────────────────────

    local scoring_result
    if ! scoring_result=$(bash "$SCORING_ENGINE" "$source" "$signal" "$weight" "$context" 2>>"$APRIL_LOG"); then
        _log "SCORING_ENGINE_FAILED src=${source} signal=${signal} — using fallback score=0"
        scoring_result="${source}|0|CLEAN|0"
    fi

    local _src score verdict_state score_delta
    IFS='|' read -r _src score verdict_state score_delta <<< "$scoring_result"

    _log "SCORED src=${source} signal=${signal} score=${score} state=${verdict_state} delta=${score_delta}"

    # ── Build case JSON ────────────────────────────────────────────────────────

    local case_id="case_${NOW}"
    local case_file="${CRE_DIR}/cases/${case_id}.json"

    local evidence_bundle
    evidence_bundle=$(_build_evidence_bundle \
        "$source" "$signal" "$weight" "$score" "$verdict_state" \
        "$score_delta" "$EPOCH_S" "$context" "$daemon_source")

    cat > "$case_file" <<JSON
{
  "case_id": "${case_id}",
  "source": "${source}",
  "signal": "${signal}",
  "base_weight": ${weight},
  "epoch": ${EPOCH_S},
  "verdict_state": "${verdict_state}",
  "evidence": ${evidence_bundle}
}
JSON

    _log "CASE_ASSEMBLED case_id=${case_id} src=${source} state=${verdict_state} score=${score}"

    # ── Route to judge_executor ────────────────────────────────────────────────
    # Only escalate if state is above CLEAN / WATCHED
    # WATCHED is logged only — no enforcement routing needed

    case "$verdict_state" in
        CLEAN|WATCHED)
            _log "NO_ROUTE state=${verdict_state} — below enforcement threshold"
            ;;
        *)
            echo "${case_id}|${source}|${verdict_state}|${score}" >> "$JUDGE_PIPE"
            _log "ROUTED_TO_JUDGE case_id=${case_id} state=${verdict_state}"
            ;;
    esac
}

# ── Main loop ─────────────────────────────────────────────────────────────────

_log "START listening on ${EXECUTION_PIPE}"

[[ -p "$EXECUTION_PIPE" ]] || mkfifo "$EXECUTION_PIPE"
[[ -p "$JUDGE_PIPE"     ]] || mkfifo "$JUDGE_PIPE"

while true; do
    while IFS='|' read -r SOURCE SIGNAL WEIGHT CONTEXT; do
    [[ -z "$SOURCE" || -z "$SIGNAL" ]] && continue

    if [[ -f "$IA_LOCK" ]]; then
        _log "SUSPENDED src=${SOURCE} — pipeline locked by internal_affairs"
        continue
    fi

    _assemble_case "$SOURCE" "$SIGNAL" "$WEIGHT" "$CONTEXT" &

done < "$EXECUTION_PIPE"
done
