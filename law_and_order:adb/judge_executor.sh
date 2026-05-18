#!/data/data/com.termux/files/usr/bin/bash
# judge_executor.sh — Judge Executor v2
# Part of MiuiserPeruser Judicial System v2
#
# CHANGES FROM v1:
#   - 5-state machine: CLEAN/WATCHED/WARNED/QUARANTINED/HOUSE_ARREST/JAILED
#   - Routes through consent_gate.sh before any enforcement
#   - Sovereignty cap enforced (cap at QUARANTINED, never auto-escalate)
#   - Emits structured VERDICT entries to court.events
#   - Writes to criminal record on JAILED / QUARANTINED verdicts
#   - MiuiserPeruser daemon verdicts routed to internal_affairs first
#
# Reads from: judgement.pipe  (written to by april_o_neil.sh)
#   Format:   case_id|source|verdict_state|score
#
# Calls:      consent_gate.sh → superhero_adapter.sh
# Writes:     state/court.events, state/criminal_record/ledger.log
#             cre/judge_executor.log

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LAW_DIR="${BASE_DIR}/law_and_order:adb"
PIPES_DIR="${BASE_DIR}/pipes"
STATE_DIR="${BASE_DIR}/state"
CRE_DIR="${BASE_DIR}/cre"

JUDGE_PIPE="${PIPES_DIR}/judgement.pipe"
COURT_EVENTS="${STATE_DIR}/court.events"
COURT_EVENTS_LOCK="${STATE_DIR}/court.events.lock"
CRIMINAL_LEDGER="${STATE_DIR}/criminal_record/ledger.log"
CRIMINAL_LEDGER_LOCK="${STATE_DIR}/criminal_record/ledger.log.lock"
JUDGE_LOG="${CRE_DIR}/judge_executor.log"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"

CONSENT_GATE="${LAW_DIR}/consent_gate.sh"
SUPERHERO_ADAPTER="${LAW_DIR}/superhero_adapter.sh"

# Source tier lib for daemon routing
# shellcheck source=source_tier_lib.sh
source "${LAW_DIR}/source_tier_lib.sh"

mkdir -p "${STATE_DIR}/criminal_record" "$CRE_DIR"
touch "$JUDGE_LOG" "$COURT_EVENTS" "$CRIMINAL_LEDGER"

# ── PID management ────────────────────────────────────────────────────────────

PID_FILE="${PIPES_DIR}/pids/judge_executor.pid"
mkdir -p "$(dirname "$PID_FILE")"
echo $$ > "$PID_FILE"
trap 'rm -f "$PID_FILE"; exit 0' EXIT INT TERM HUP

# ── Logging ───────────────────────────────────────────────────────────────────

_log() {
    echo "[JUDGE] $(date +%s) $*" >> "$JUDGE_LOG"
}

# ── Court events emitter ──────────────────────────────────────────────────────

_emit_verdict() {
    local epoch="$1" source="$2" verdict="$3" case_id="$4" score="$5"
    local entry="${epoch}|VERDICT|${source}|${verdict}|${case_id}|score=${score}"
    (
        flock -x 200
        echo "$entry" >> "$COURT_EVENTS"
    ) 200>"$COURT_EVENTS_LOCK"
    _log "VERDICT src=${source} verdict=${verdict} score=${score} case=${case_id}"
}

_emit_pending() {
    local epoch="$1" source="$2" case_id="$3"
    local entry="${epoch}|PENDING_JUDGEMENT|${source}||${case_id}|"
    (
        flock -x 200
        echo "$entry" >> "$COURT_EVENTS"
    ) 200>"$COURT_EVENTS_LOCK"
}

_clear_pending() {
    local case_id="$1"
    local tmp
    tmp=$(mktemp)
    grep -v "PENDING_JUDGEMENT.*${case_id}" "$COURT_EVENTS" > "$tmp" || true
    mv "$tmp" "$COURT_EVENTS"
}

# ── Criminal record writer ────────────────────────────────────────────────────
# Only on JAILED and QUARANTINED verdicts.
# Format: epoch|source|verdict|case_id|score

_write_criminal_record() {
    local source="$1" verdict="$2" case_id="$3" score="$4"
    local entry="$(date +%s)|${source}|${verdict}|${case_id}|${score}"
    (
        flock -x 200
        echo "$entry" >> "$CRIMINAL_LEDGER"
    ) 200>"$CRIMINAL_LEDGER_LOCK"
}

# ── Enforcement dispatcher ────────────────────────────────────────────────────

_enforce() {
    local source="$1" verdict_state="$2" case_id="$3" score="$4"

    local action
    case "$verdict_state" in
        WARNED)       action="INTERVENE"   ;;
        QUARANTINED)  action="ISOLATE"     ;;
        HOUSE_ARREST) action="RESTRICT"    ;;
        JAILED)       action="KILL"        ;;
        *)
            _log "NO_ACTION state=${verdict_state} src=${source}"
            return 0
            ;;
    esac

    _log "ENFORCE src=${source} action=${action} case=${case_id}"

    if [[ -x "$SUPERHERO_ADAPTER" ]]; then
        bash "$SUPERHERO_ADAPTER" "$action" "$source" "$case_id" 2>>"$JUDGE_LOG" || \
            _log "ADAPTER_ERROR src=${source} action=${action} rc=$?"
    else
        _log "ADAPTER_UNAVAILABLE action=${action} src=${source} — would enforce: ${action}"
    fi
}

# ── Verdict processor ─────────────────────────────────────────────────────────

_process_verdict() {
    local case_id="$1" source="$2" verdict_state="$3" score="$4"
    local NOW
    NOW=$(date +%s)

    # Pipeline suspend check
    if [[ -f "$IA_LOCK" ]]; then
        _log "SUSPENDED case=${case_id} src=${source} — internal_affairs.lock active"
        return 0
    fi

    _emit_pending "$NOW" "$source" "$case_id"

    # ── MiuiserPeruser daemon routing ─────────────────────────────────────────
    if is_miuiser_daemon "$source"; then
        _log "DAEMON_CASE src=${source} verdict=${verdict_state} — routing to internal affairs"
        _emit_verdict "$NOW" "$source" "IA_REFERRAL" "$case_id" "$score"
        _clear_pending "$case_id"
        # Signal internal_affairs to review (non-blocking)
        local ia_pid_file="${PIPES_DIR}/pids/internal_affairs.pid"
        if [[ -f "$ia_pid_file" ]]; then
            kill -USR1 "$(cat "$ia_pid_file")" 2>/dev/null || true
        fi
        return 0
    fi

    # ── CLEAN / WATCHED — no enforcement ─────────────────────────────────────
    case "$verdict_state" in
        CLEAN|WATCHED)
            _log "DISMISSED src=${source} state=${verdict_state} score=${score}"
            _emit_verdict "$NOW" "$source" "DISMISSED" "$case_id" "$score"
            _clear_pending "$case_id"
            return 0
            ;;
    esac

    # ── Sovereignty cap check ─────────────────────────────────────────────────
    if is_sovereignty_app "$source"; then
        case "$verdict_state" in
            JAILED|HOUSE_ARREST)
                _log "SOVEREIGNTY_CAP src=${source} ${verdict_state}→QUARANTINED"
                verdict_state="QUARANTINED"
                ;;
        esac
    fi

    # ── Ethical floor: JAILED requires prior HOUSE_ARREST or QUARANTINED ──────
    if [[ "$verdict_state" == "JAILED" ]]; then
        local had_prior=false
        if grep -qE "^[^|]+\|${source}\|(HOUSE_ARREST|QUARANTINED)\|" \
            "$CRIMINAL_LEDGER" 2>/dev/null; then
            had_prior=true
        fi
        if grep -qE "^\S+\|VERDICT\|${source}\|(HOUSE_ARREST|QUARANTINED)\|" \
            "$COURT_EVENTS" 2>/dev/null; then
            had_prior=true
        fi

        if [[ "$had_prior" == false ]]; then
            _log "ETHICAL_FLOOR src=${source} no prior containment — capping JAILED→HOUSE_ARREST"
            verdict_state="HOUSE_ARREST"
        fi
    fi

    # ── Consent gate ──────────────────────────────────────────────────────────
    local consent_rc=0
    if [[ -x "$CONSENT_GATE" ]]; then
        bash "$CONSENT_GATE" "$source" "$verdict_state" "$score" "$case_id"
        consent_rc=$?
    fi

    case $consent_rc in
        0)  # Approved / auto-proceed
            ;;
        1)  # Denied
            _log "CONSENT_DENIED src=${source} verdict=${verdict_state} case=${case_id}"
            _emit_verdict "$NOW" "$source" "CONSENT_DENIED" "$case_id" "$score"
            _clear_pending "$case_id"
            return 0
            ;;
        2)  # Deferred
            _log "CONSENT_DEFERRED src=${source} verdict=${verdict_state} case=${case_id}"
            _emit_verdict "$NOW" "$source" "DEFERRED" "$case_id" "$score"
            _clear_pending "$case_id"
            return 0
            ;;
        3)  # Sovereignty hard block / cap
            # Consent gate already capped verdict — re-read sovereignty cap
            verdict_state="QUARANTINED"
            _log "SOVEREIGNTY_HARD_CAP src=${source} capped to QUARANTINED"
            ;;
    esac

    # ── Enforce ───────────────────────────────────────────────────────────────
    _enforce "$source" "$verdict_state" "$case_id" "$score"

    # ── Emit verdict and criminal record ─────────────────────────────────────
    _emit_verdict "$NOW" "$source" "$verdict_state" "$case_id" "$score"

    case "$verdict_state" in
        JAILED|QUARANTINED)
            _write_criminal_record "$source" "$verdict_state" "$case_id" "$score"
            ;;
    esac

    _clear_pending "$case_id"
}

# ── Main loop ─────────────────────────────────────────────────────────────────

_log "START 5-state machine listening on ${JUDGE_PIPE}"

[[ -p "$JUDGE_PIPE" ]] || mkfifo "$JUDGE_PIPE"

while IFS='|' read -r CASE_ID SOURCE VERDICT_STATE SCORE; do
    [[ -z "$CASE_ID" || -z "$SOURCE" ]] && continue
    _process_verdict "$CASE_ID" "$SOURCE" "$VERDICT_STATE" "$SCORE" &
done < <(tail -f "$JUDGE_PIPE")
