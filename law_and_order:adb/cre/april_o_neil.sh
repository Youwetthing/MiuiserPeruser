#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}

# april_o_neil.sh — Case Reporter & Evidence Enrichment v2
# Part of MiuiserPeruser Judicial System v2

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

LAW_DIR="${BASE_DIR}/law_and_order:adb"
PIPES_DIR="${BASE_DIR}/pipes"
CRE_DIR="${BASE_DIR}/cre"
STATE_DIR="${BASE_DIR}/state"

INGEST_PIPE="${PIPES_DIR}/ingest.pipe"
APRIL_LOG="${CRE_DIR}/april.log"
SCORING_ENGINE="${LAW_DIR}/score_query.sh"
JUDGE_PIPE="${PIPES_DIR}/judgement.pipe"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"

mkdir -p "$CRE_DIR/cases" "$(dirname "$APRIL_LOG")"
touch "$APRIL_LOG"

[[ -p "$INGEST_PIPE" ]]  || mkfifo "$INGEST_PIPE"
[[ -p "$JUDGE_PIPE"  ]]  || mkfifo "$JUDGE_PIPE"

_log() {
    echo "[APRIL] $(date +%s) $*" >> "$APRIL_LOG"
}

# ── Evidence bundle ───────────────────────────────────────────────────────────

_build_evidence_bundle() {
    local source="$1" signal="$2" weight="$3" score="$4"
    local verdict_state="$5" score_delta="$6" case_epoch="$7"
    local context="$8" daemon_source="$9"

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

    local reasoning_ref=""
    if [[ -f "${CRE_DIR}/scoring_reasoning.log" ]]; then
        reasoning_ref=$(grep "src=${source} signal=${signal}" \
            "${CRE_DIR}/scoring_reasoning.log" | tail -1 || true)
    fi

    python3 - << PYEOF
import json, sys
reasoning = """${reasoning_ref}"""
context   = """${context}"""
score     = ${score}
delta     = ${score_delta}
weight    = ${weight}
print(json.dumps({
    "score":          score,
    "score_delta":    delta,
    "verdict_state":  "${verdict_state}",
    "base_weight":    weight,
    "daemon_source":  "${daemon_source}",
    "reasoning_ref":  reasoning.strip(),
    "context":        context.strip()
}))
PYEOF
}

# ── Case assembler ────────────────────────────────────────────────────────────

_assemble_case() {
    local source="$1" signal="$2" weight="$3" context="${4:-}"

    # Guard — skip blank or obviously malformed sources
    [[ -z "$source" || "$source" =~ ^[0-9]+$ ]] && return 0
    [[ "$source" == "0" ]] && return 0

    local NOW
    NOW=$(date +%s%3N)
    local EPOCH_S=$(( NOW / 1000 ))

    local daemon_source
    daemon_source=$(echo "$context" | grep -oP '(?<=daemon=)[^;]+' 2>/dev/null || echo "unknown")

    # ── Score via scoring_engine ──────────────────────────────────────────────
    local scoring_result score verdict_state score_delta _src

    scoring_result=$(timeout 8 bash "$SCORING_ENGINE" \
        "$source" "$signal" "$weight" "$context" 2>>"$APRIL_LOG") || {
        _log "SCORING_ENGINE_FAILED src=${source} signal=${signal}"
        return 0
    }

    IFS='|' read -r _src score verdict_state score_delta <<< "$scoring_result"

    # Sanitise score fields
    score=$(printf "%.2f" "${score:-0}" 2>/dev/null || echo "0")
    score_delta=$(printf "%.2f" "${score_delta:-0}" 2>/dev/null || echo "0")
    verdict_state="${verdict_state:-CLEAN}"

    _log "SCORED src=${source} signal=${signal} score=${score} state=${verdict_state} delta=${score_delta}"

    # ── Build case JSON ───────────────────────────────────────────────────────
    local case_id="case_${NOW}"
    local case_file="${CRE_DIR}/cases/${case_id}.json"

    local evidence_bundle
    evidence_bundle=$(_build_evidence_bundle \
        "$source" "$signal" "$weight" "$score" "$verdict_state" \
        "$score_delta" "$EPOCH_S" "$context" "$daemon_source") || evidence_bundle="{}"

    python3 - << PYEOF > "$case_file"
import json
print(json.dumps({
    "case_id":      "${case_id}",
    "source":       "${source}",
    "signal":       "${signal}",
    "base_weight":  ${weight},
    "epoch":        ${EPOCH_S},
    "verdict_state":"${verdict_state}",
    "evidence":     ${evidence_bundle}
}, indent=2))
PYEOF

    _log "CASE_ASSEMBLED case_id=${case_id} src=${source} state=${verdict_state} score=${score}"

    # ── Route to judge_executor ───────────────────────────────────────────────
    case "$verdict_state" in
        CLEAN|WATCHED)
            _log "NO_ROUTE state=${verdict_state} — below enforcement threshold"
            ;;
        *)
            echo "${case_id}|${source}|${verdict_state}|${score}" > "$JUDGE_PIPE" &
            _log "ROUTED_TO_JUDGE case_id=${case_id} state=${verdict_state}"
            ;;
    esac
}

# ── Main loop ─────────────────────────────────────────────────────────────────
# Uses a persistent fd on the FIFO so the loop never exits on EOF.
# Writer closes → fd stays open → read blocks cleanly until next write.

_log "START listening on ${INGEST_PIPE}"

exec 7<> "$INGEST_PIPE"

while IFS='|' read -r SOURCE SIGNAL WEIGHT CONTEXT <&7; do
    [[ -z "$SOURCE" || -z "$SIGNAL" ]] && continue

    if [[ -f "$IA_LOCK" ]]; then
        _log "SUSPENDED src=${SOURCE} — pipeline locked by internal_affairs"
        continue
    fi

    _assemble_case "$SOURCE" "$SIGNAL" "$WEIGHT" "$CONTEXT" &

done
