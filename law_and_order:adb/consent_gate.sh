#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
# consent_gate.sh — Consent Gate v2
# Part of MiuiserPeruser Judicial System v2
#
# Sits between judge_executor.sh verdict and superhero_adapter.sh enforcement.
# Called by judge_executor.sh before any enforcement action.
#
# Usage:
#   consent_gate.sh <source> <verdict_state> <score> <case_id>
#
# Exit codes:
#   0 — Consent granted (proceed to enforcement)
#   1 — Consent denied (abort enforcement)
#   2 — Deferred (hold, re-notify later)
#   3 — Sovereignty hard block (never enforce without explicit approval)
#
# Side effects:
#   - May write to state/consent_queue.state (dashboard fallback)
#   - Writes decisions to cre/judge_executor.log

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

STATE_DIR="${BASE_DIR}/state"
CRE_DIR="${BASE_DIR}/cre"

CONSENT_QUEUE="${STATE_DIR}/consent_queue.state"
SOVEREIGNTY_FILE="${STATE_DIR}/sovereignty.list"
CONSENT_LOG="${CRE_DIR}/judge_executor.log"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"

# Source tier lib for sovereignty check
# shellcheck source=source_tier_lib.sh
source "${SCRIPT_DIR}/source_tier_lib.sh"

# ── Args ──────────────────────────────────────────────────────────────────────

SOURCE="${1:-}"
VERDICT_STATE="${2:-}"
SCORE="${3:-0}"
CASE_ID="${4:-unknown}"

if [[ -z "$SOURCE" || -z "$VERDICT_STATE" ]]; then
    echo "Usage: consent_gate.sh <source> <verdict_state> <score> <case_id>" >&2
    exit 1
fi

NOW=$(date +%s)

mkdir -p "$STATE_DIR" "$CRE_DIR"
touch "$CONSENT_QUEUE" "$CONSENT_LOG"

# ── Logging ───────────────────────────────────────────────────────────────────

_log() {
    echo "[CONSENT] ${NOW} src=${SOURCE} verdict=${VERDICT_STATE} score=${SCORE} case=${CASE_ID} $*" \
        >> "$CONSENT_LOG"
}

# ── Pipeline suspend check ────────────────────────────────────────────────────

if [[ -f "$IA_LOCK" ]]; then
    _log "BLOCKED_BY_IA pipeline suspended by internal_affairs.lock"
    exit 2
fi

# ── Determine whether consent is required ────────────────────────────────────
# Rules (from blueprint):
#   Sovereignty app            → always require consent, QUARANTINE max
#   MiuiserPeruser daemon      → internal affairs first, not user consent
#   WARNED / INTERVENE         → no consent required (auto-proceed after 10min timeout)
#   QUARANTINED and above      → consent required for sovereignty apps; others auto-proceed with timeout
#   JAILED / KILL              → always require explicit consent

_requires_consent() {
    # KILL always requires consent — no timeout auto-proceed
    [[ "$VERDICT_STATE" == "JAILED" ]] && return 0

    # Sovereignty apps always require consent
    is_sovereignty_app "$SOURCE" && return 0

    # MiuiserPeruser daemons go to internal affairs, not user consent
    if is_miuiser_daemon "$SOURCE"; then
        _log "MIUISER_DAEMON routed to internal affairs gate"
        return 1
    fi

    # HOUSE_ARREST and QUARANTINED for non-sovereignty — consent required
    [[ "$VERDICT_STATE" == "HOUSE_ARREST" ]] && return 0
    [[ "$VERDICT_STATE" == "QUARANTINED" ]] && return 0

    # WARNED / WATCHED / CLEAN — no consent gate
    return 1
}

# ── Timeout policy ────────────────────────────────────────────────────────────
# Returns seconds to wait before auto action, and what action (proceed/hold)

_timeout_policy() {
    case "$VERDICT_STATE" in
        WARNED)       echo "600|proceed"   ;;   # 10 min → auto proceed
        QUARANTINED)  echo "1200|hold"     ;;   # 20 min → stay in hold, re-notify
        HOUSE_ARREST) echo "1800|hold"     ;;   # 30 min → stay in hold, re-notify
        JAILED)       echo "0|require"     ;;   # No auto — explicit approval always
        *)            echo "600|proceed"   ;;
    esac
}

# ── Sovereignty hard block ────────────────────────────────────────────────────

_check_sovereignty_cap() {
    # Sovereignty apps: QUARANTINE is the maximum allowed verdict
    is_sovereignty_app "$SOURCE" || return 0

    case "$VERDICT_STATE" in
        JAILED|HOUSE_ARREST)
            _log "SOVEREIGNTY_CAP verdict=${VERDICT_STATE} capped to QUARANTINED"
            echo "SOVEREIGNTY_CAP:QUARANTINED"
            return 0
            ;;
    esac
}

# ── Write to consent queue (dashboard fallback) ────────────────────────────────
# Format: case_id|source|verdict_state|score|queued_epoch|timeout_secs|timeout_action

_queue_consent_request() {
    local timeout_secs="$1"
    local timeout_action="$2"
    local entry
    entry="${CASE_ID}|${SOURCE}|${VERDICT_STATE}|${SCORE}|${NOW}|${timeout_secs}|${timeout_action}"

    # Remove any existing pending entry for this source
    if [[ -s "$CONSENT_QUEUE" ]]; then
        grep -v "^[^|]*|${SOURCE}|" "$CONSENT_QUEUE" > "${CONSENT_QUEUE}.tmp" || true
        mv "${CONSENT_QUEUE}.tmp" "$CONSENT_QUEUE"
    fi

    echo "$entry" >> "$CONSENT_QUEUE"
    _log "QUEUED_TO_DASHBOARD timeout=${timeout_secs}s action=${timeout_action}"
}

# ── Send termux-notification ───────────────────────────────────────────────────

_send_notification() {
    local timeout_secs="$1"
    local timeout_action="$2"

    # Human-friendly verdict labels
    local action_label
    case "$VERDICT_STATE" in
        JAILED)       action_label="KILL (terminate process)" ;;
        HOUSE_ARREST) action_label="HOUSE ARREST (restrict operations)" ;;
        QUARANTINED)  action_label="QUARANTINE (isolate process)" ;;
        WARNED)       action_label="INTERVENE (raise alert)" ;;
        *)            action_label="$VERDICT_STATE" ;;
    esac

    local score_int
    score_int=$(printf "%.0f" "$SCORE")

    local body="Source: ${SOURCE}
Score: ${score_int}/100  |  Proposed: ${action_label}
Case ID: ${CASE_ID}"

    if [[ "$VERDICT_STATE" == "JAILED" ]]; then
        body="${body}
⚠️  KILL requires your explicit approval. No timeout."
    else
        local timeout_min=$(( timeout_secs / 60 ))
        body="${body}
Auto-action in ${timeout_min} min if no response: ${timeout_action}"
    fi

    # termux-notification with action buttons
    if command -v termux-notification &>/dev/null; then
        termux-notification \
            --id "miuiser_consent_${CASE_ID}" \
            --title "MiuiserPeruser — Action Required" \
            --content "$body" \
            --action  "echo APPROVED  > ${STATE_DIR}/consent_reply_${CASE_ID}" \
            --button1 "Approve" \
            --button1-action "echo APPROVED  > ${STATE_DIR}/consent_reply_${CASE_ID}" \
            --button2 "Deny" \
            --button2-action "echo DENIED    > ${STATE_DIR}/consent_reply_${CASE_ID}" \
            --button3 "Defer 10min" \
            --button3-action "echo DEFERRED  > ${STATE_DIR}/consent_reply_${CASE_ID}" \
            --priority "high" \
            2>/dev/null && \
            _log "NOTIFICATION_SENT" || \
            _log "NOTIFICATION_FAILED fallback=queue"
    else
        _log "TERMUX_NOTIFICATION_UNAVAILABLE fallback=queue"
    fi
}

# ── Wait for user reply ───────────────────────────────────────────────────────
# Polls for consent_reply file. Returns 0/1/2 (approved/denied/deferred/timeout)

_wait_for_reply() {
    local timeout_secs="$1"
    local timeout_action="$2"
    local reply_file="${STATE_DIR}/consent_reply_${CASE_ID}"
    local deadline=$(( NOW + timeout_secs ))

    # JAILED: no timeout — wait indefinitely (up to 24h then force hold)
    if [[ "$VERDICT_STATE" == "JAILED" ]]; then
        deadline=$(( NOW + 86400 ))
    fi

    while true; do
        if [[ -f "$reply_file" ]]; then
            local reply
            reply=$(cat "$reply_file" | tr -d '[:space:]')
            rm -f "$reply_file"
            case "$reply" in
                APPROVED)  _log "USER_APPROVED"; return 0 ;;
                DENIED)    _log "USER_DENIED";   return 1 ;;
                DEFERRED)
                    _log "USER_DEFERRED 10min"
                    _queue_consent_request "600" "$timeout_action"
                    return 2
                    ;;
            esac
        fi

        local now_check
        now_check=$(date +%s)

        if [[ $now_check -ge $deadline ]]; then
            case "$timeout_action" in
                proceed)
                    _log "TIMEOUT_AUTO_PROCEED timeout=${timeout_secs}s"
                    return 0
                    ;;
                hold)
                    _log "TIMEOUT_RENOTIFY staying in hold"
                    # Re-notify and reset timer
                    _send_notification "$timeout_secs" "$timeout_action"
                    deadline=$(( now_check + timeout_secs ))
                    ;;
                require)
                    # Should not reach here for JAILED — but safety valve
                    _log "TIMEOUT_HOLD_JAILED re-notifying"
                    _send_notification "0" "require"
                    deadline=$(( now_check + 3600 ))
                    ;;
            esac
        fi

        sleep 5
    done
}

# ── Main gate logic ───────────────────────────────────────────────────────────

# Check sovereignty cap first
SOV_CAP=$(_check_sovereignty_cap || true)
if [[ -n "$SOV_CAP" ]]; then
    # Emit capped verdict — caller should re-route to QUARANTINED max
    echo "SOVEREIGNTY_CAP"
    exit 3
fi

# Does this verdict require consent?
if ! _requires_consent; then
    _log "CONSENT_NOT_REQUIRED auto_proceed"
    exit 0
fi

# Get timeout policy
IFS='|' read -r TIMEOUT_SECS TIMEOUT_ACTION <<< "$(_timeout_policy)"

# Write to dashboard queue (always — notification may fail)
_queue_consent_request "$TIMEOUT_SECS" "$TIMEOUT_ACTION"

# Send notification
_send_notification "$TIMEOUT_SECS" "$TIMEOUT_ACTION"

# Wait for response
_wait_for_reply "$TIMEOUT_SECS" "$TIMEOUT_ACTION"
EXIT_CODE=$?

case $EXIT_CODE in
    0) exit 0 ;;   # Approved / auto-proceed
    1) exit 1 ;;   # Denied
    2) exit 2 ;;   # Deferred
    *) exit 2 ;;
esac
