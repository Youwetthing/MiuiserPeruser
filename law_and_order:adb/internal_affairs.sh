#!/data/data/com.termux/files/usr/bin/bash
# internal_affairs.sh — Internal Affairs Daemon v2
# Part of MiuiserPeruser Judicial System v2
#
# Runs completely outside the judicial pipeline. Read-only access to all
# judicial state. Can suspend the pipeline by writing state/internal_affairs.lock
# but CANNOT itself issue verdicts.
#
# Audit cycle: every 5 minutes.
#
# Usage:
#   internal_affairs.sh [--once|--daemon|--status|--clear-lock]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

STATE_DIR="${BASE_DIR}/state"
CRE_DIR="${BASE_DIR}/cre"
LOG_DIR="${BASE_DIR}/logs"

THREAT_SCORES_STATE="${STATE_DIR}/threat_scores.state"
COURT_EVENTS="${STATE_DIR}/court.events"
CONSENT_QUEUE="${STATE_DIR}/consent_queue.state"
CRIMINAL_LEDGER="${STATE_DIR}/criminal_record/ledger.log"
IA_LOG="${CRE_DIR}/internal_affairs.log"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"
SCORING_LOG="${CRE_DIR}/scoring_reasoning.log"
JUDGE_LOG="${CRE_DIR}/judge_executor.log"

AUDIT_INTERVAL=300   # 5 minutes
FLAG_WINDOW=3600     # 1 hour window for repeated flag detection
REPEAT_FLAG_THRESHOLD=3

ONCE_MODE=false
[[ "${1:-}" == "--once" ]] && ONCE_MODE=true

# ── PID management ────────────────────────────────────────────────────────────

PID_FILE="${BASE_DIR}/pipes/pids/internal_affairs.pid"
mkdir -p "$(dirname "$PID_FILE")" "$STATE_DIR" "$CRE_DIR"
touch "$IA_LOG"

if [[ "$ONCE_MODE" == false && "${1:-}" == "--daemon" ]]; then
    if [[ -f "$PID_FILE" ]]; then
        OLD_PID=$(cat "$PID_FILE")
        if kill -0 "$OLD_PID" 2>/dev/null; then
            echo "[internal_affairs] Already running (pid $OLD_PID)" >&2
            exit 0
        fi
    fi
    echo $$ > "$PID_FILE"
    trap 'rm -f "$PID_FILE"; exit 0' EXIT INT TERM
fi

NOW=$(date +%s)
WINDOW_START=$(( NOW - FLAG_WINDOW ))

# ── Logging ───────────────────────────────────────────────────────────────────
# Format: [IA] epoch | CHECK | result | detail

_ia_log() {
    local check="$1" result="$2" detail="${3:-}"
    echo "[IA] $(date +%s) | ${check} | ${result} | ${detail}" >> "$IA_LOG"
}

_ia_flag() {
    local check="$1" detail="$2"
    _ia_log "$check" "FLAG" "$detail"
}

_ia_pass() {
    local check="$1" detail="${2:-}"
    _ia_log "$check" "PASS" "$detail"
}

_ia_critical() {
    local check="$1" detail="$2"
    _ia_log "$check" "CRITICAL" "$detail"
    _suspend_pipeline "$check: $detail"
    _notify_user "CRITICAL: ${check}" "$detail"
}

# ── Pipeline suspend ──────────────────────────────────────────────────────────

_suspend_pipeline() {
    local reason="$1"
    echo "$(date +%s)|${reason}" > "$IA_LOCK"
    _ia_log "PIPELINE_SUSPEND" "CRITICAL" "reason=${reason}"
    echo "[IA] PIPELINE SUSPENDED: $reason" >&2
}

_clear_lock() {
    if [[ -f "$IA_LOCK" ]]; then
        rm -f "$IA_LOCK"
        _ia_log "PIPELINE_RESUME" "MANUAL" "lock cleared by operator"
        echo "[IA] Pipeline lock cleared."
    else
        echo "[IA] No lock active."
    fi
}

# ── User notification ─────────────────────────────────────────────────────────

_notify_user() {
    local title="$1" body="$2"
    if command -v termux-notification &>/dev/null; then
        termux-notification \
            --id "miuiser_ia_$(date +%s)" \
            --title "MiuiserPeruser Internal Affairs: ${title}" \
            --content "$body" \
            --priority "high" \
            2>/dev/null || true
    fi
}

# ── Repeat flag detector ───────────────────────────────────────────────────────
# Returns count of same CHECK flags in the last hour

_repeat_flag_count() {
    local check="$1"
    grep -c "^\[IA\] .* | ${check} | FLAG |" "$IA_LOG" 2>/dev/null | \
        awk -v ws="$WINDOW_START" 'BEGIN{count=0}
        {if ($2+0 > ws) count++} END{print count+0}' || echo 0
}

# ── Audit checks ──────────────────────────────────────────────────────────────

# CHECK 1: Verdict rate anomaly
# If JAILED verdicts > 30% of all verdicts in last hour → flag
_check_verdict_rate() {
    [[ -f "$COURT_EVENTS" ]] || { _ia_pass "VERDICT_RATE" "no court.events yet"; return; }

    local total jailed pct
    total=$(awk -F'|' -v ws="$WINDOW_START" '$1 > ws && $2 == "VERDICT"' \
        "$COURT_EVENTS" 2>/dev/null | wc -l || echo 0)
    jailed=$(awk -F'|' -v ws="$WINDOW_START" \
        '$1 > ws && $2 == "VERDICT" && $4 == "JAILED"' \
        "$COURT_EVENTS" 2>/dev/null | wc -l || echo 0)

    if [[ $total -gt 0 ]]; then
        pct=$(awk -v j="$jailed" -v t="$total" 'BEGIN { printf "%d", (j/t)*100 }')
        if [[ $pct -gt 30 ]]; then
            _ia_flag "VERDICT_RATE" "JAILED=${jailed}/${total} (${pct}%) in last hour — threshold 30%"
            local repeat_count
            repeat_count=$(_repeat_flag_count "VERDICT_RATE")
            if [[ $repeat_count -ge $REPEAT_FLAG_THRESHOLD ]]; then
                _notify_user "Verdict Rate Alert" \
                    "JAILED verdicts at ${pct}% (${jailed}/${total}) — may indicate scoring bias"
            fi
        else
            _ia_pass "VERDICT_RATE" "JAILED=${jailed}/${total} (${pct}%)"
        fi
    else
        _ia_pass "VERDICT_RATE" "no verdicts in last hour"
    fi

    # Sub-check: score ceiling hits
    local ceiling_hits
    ceiling_hits=$(awk -F'|' '$2 == "100.00" || $2 == "100"' \
        "$THREAT_SCORES_STATE" 2>/dev/null | wc -l || echo 0)
    if [[ $ceiling_hits -gt 5 ]]; then
        _ia_flag "SCORE_CEILING" "${ceiling_hits} sources simultaneously at score=100"
    fi
}

# CHECK 2: Score inflation
# If average score across all sources > 60 for > 10 minutes → flag

_check_score_inflation() {
    [[ -s "$THREAT_SCORES_STATE" ]] || { _ia_pass "SCORE_INFLATION" "no scores yet"; return; }

    local avg_score
    avg_score=$(awk -F'|' \
        '{sum += $2; count++} END { if (count > 0) printf "%.1f", sum/count; else print 0 }' \
        "$THREAT_SCORES_STATE" 2>/dev/null || echo 0)

    if awk -v avg="$avg_score" 'BEGIN { exit (avg > 60) ? 0 : 1 }'; then
        _ia_flag "SCORE_INFLATION" "avg_score=${avg_score} across all sources (threshold 60)"
        local repeat_count
        repeat_count=$(_repeat_flag_count "SCORE_INFLATION")
        if [[ $repeat_count -ge 2 ]]; then
            # 2 cycles × 5min = 10+ minutes of sustained inflation
            _notify_user "Score Inflation Detected" \
                "Average threat score is ${avg_score}/100 — decay may not be working"
        fi
    else
        _ia_pass "SCORE_INFLATION" "avg_score=${avg_score}"
    fi
}

# CHECK 3: Enforcement without evidence
# Every KILL/ISOLATE cross-references case file — if case has < 2 signals → flag

_check_enforcement_evidence() {
    [[ -f "$COURT_EVENTS" ]] || { _ia_pass "ENFORCEMENT_EVIDENCE" "no court.events yet"; return; }

    local violations=0

    while IFS='|' read -r epoch etype source verdict case_id rest; do
        [[ "$etype" == "VERDICT" ]] || continue
        [[ "$verdict" == "JAILED" || "$verdict" == "QUARANTINED" ]] || continue

        # Check corresponding case file
        local case_file
        case_file=$(find "${CRE_DIR}/cases" -name "${case_id}.json" 2>/dev/null | head -1)

        if [[ -z "$case_file" || ! -f "$case_file" ]]; then
            _ia_flag "ENFORCEMENT_EVIDENCE" \
                "verdict=${verdict} src=${source} case=${case_id} NO_CASE_FILE"
            (( violations++ )) || true
            continue
        fi

        # Count distinct signals in case file
        local signal_count
        signal_count=$(grep -c '"signal"' "$case_file" 2>/dev/null || echo 0)

        if [[ $signal_count -lt 2 ]]; then
            _ia_flag "ENFORCEMENT_EVIDENCE" \
                "verdict=${verdict} src=${source} case=${case_id} signals=${signal_count} (min 2)"
            (( violations++ )) || true

            if [[ "$verdict" == "JAILED" ]]; then
                _ia_critical "ENFORCEMENT_EVIDENCE" \
                    "KILL verdict with only ${signal_count} signal(s) — ethical floor breach"
            fi
        fi

    done < <(awk -F'|' -v ws="$WINDOW_START" '$1 > ws' "$COURT_EVENTS" 2>/dev/null || true)

    [[ $violations -eq 0 ]] && _ia_pass "ENFORCEMENT_EVIDENCE" "all verdicts corroborated"
}

# CHECK 4: Pipeline stalling
# Cases in PENDING_JUDGEMENT > 5 minutes → flag, attempt restart

_check_pipeline_stall() {
    local STALL_THRESHOLD=300   # 5 minutes
    local stalled=0
    local stall_cutoff=$(( NOW - STALL_THRESHOLD ))

    [[ -f "$COURT_EVENTS" ]] || { _ia_pass "PIPELINE_STALL" "no events yet"; return; }

    while IFS='|' read -r epoch etype rest; do
        [[ "$etype" == "PENDING_JUDGEMENT" ]] || continue
        if [[ $epoch -lt $stall_cutoff ]]; then
            _ia_flag "PIPELINE_STALL" "event at epoch=${epoch} stuck in PENDING_JUDGEMENT"
            (( stalled++ )) || true
        fi
    done < <(grep "|PENDING_JUDGEMENT|" "$COURT_EVENTS" 2>/dev/null || true)

    if [[ $stalled -gt 0 ]]; then
        _notify_user "Pipeline Stall Detected" \
            "${stalled} case(s) stuck in PENDING_JUDGEMENT > 5 minutes"
        # Attempt pipeline restart signal
        local judge_pid_file="${BASE_DIR}/pipes/pids/judge_executor.pid"
        if [[ -f "$judge_pid_file" ]]; then
            local judge_pid
            judge_pid=$(cat "$judge_pid_file")
            kill -HUP "$judge_pid" 2>/dev/null && \
                _ia_log "PIPELINE_STALL" "HUP_SENT" "sent HUP to judge_executor pid=${judge_pid}" || \
                _ia_log "PIPELINE_STALL" "HUP_FAILED" "could not signal judge_executor"
        fi
    else
        _ia_pass "PIPELINE_STALL" "no stalled cases"
    fi
}

# CHECK 5: Self-exemption check
# MiuiserPeruser daemons absent from threat_scores.state for > 1 hour
# despite being active → flag (they SHOULD be scored when misbehaving)

_check_self_exemption() {
    local ABSENCE_THRESHOLD=3600

    # Read known running daemons from judicial log or pid directory
    local active_miuiser_daemons=()
    local pids_dir="${BASE_DIR}/pipes/pids"

    if [[ -d "$pids_dir" ]]; then
        while IFS= read -r pid_file; do
            local daemon_name
            daemon_name="$(basename "$pid_file" .pid)"
            local pid
            pid=$(cat "$pid_file" 2>/dev/null || true)
            if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
                active_miuiser_daemons+=("$daemon_name")
            fi
        done < <(find "$pids_dir" -name "*.pid" 2>/dev/null)
    fi

    local flagged=0
    for daemon in "${active_miuiser_daemons[@]}"; do
        if ! grep -q "^${daemon}|" "$THREAT_SCORES_STATE" 2>/dev/null; then
            # Never scored — check how long it's been running
            local pid_file="${pids_dir}/${daemon}.pid"
            local mtime
            mtime=$(stat -c %Y "$pid_file" 2>/dev/null || echo "$NOW")
            local running_for=$(( NOW - mtime ))
            if [[ $running_for -gt $ABSENCE_THRESHOLD ]]; then
                _ia_flag "SELF_EXEMPTION" \
                    "daemon=${daemon} active ${running_for}s but absent from threat_scores"
                (( flagged++ )) || true
            fi
        fi
    done

    [[ $flagged -eq 0 ]] && _ia_pass "SELF_EXEMPTION" "all active daemons accounted for"
}

# ── Ethical Floor Enforcement (hard limits — cannot be overridden) ─────────────

_check_ethical_floors() {
    local violations=0

    [[ -f "$COURT_EVENTS" ]] || { _ia_pass "ETHICAL_FLOOR" "no events yet"; return; }

    # Floor 1: KILL requires ≥ 2 corroborating signals from DIFFERENT daemon sources
    while IFS='|' read -r epoch etype source verdict case_id rest; do
        [[ "$etype" == "VERDICT" && "$verdict" == "JAILED" ]] || continue

        local case_file
        case_file=$(find "${CRE_DIR}/cases" -name "${case_id}.json" 2>/dev/null | head -1)
        [[ -f "$case_file" ]] || continue

        local distinct_daemon_sources
        distinct_daemon_sources=$(grep '"daemon_source"' "$case_file" 2>/dev/null | \
            sort -u | wc -l || echo 0)

        if [[ $distinct_daemon_sources -lt 2 ]]; then
            _ia_critical "ETHICAL_FLOOR_KILL_CORROBORATION" \
                "KILL on ${source} case=${case_id} had only ${distinct_daemon_sources} daemon source(s) — need 2"
            (( violations++ )) || true
        fi

    done < <(awk -F'|' -v ws="$WINDOW_START" '$1 > ws' "$COURT_EVENTS" 2>/dev/null || true)

    # Floor 2: No enforcement against source CLEAN for > 10 minutes without fresh signal in last 60s
    while IFS='|' read -r epoch etype source verdict case_id rest; do
        [[ "$etype" == "VERDICT" ]] || continue
        [[ "$verdict" == "JAILED" || "$verdict" == "QUARANTINED" ]] || continue

        # Check current score state
        local current_state
        current_state=$(awk -F'|' -v s="$source" '$1==s {print $4; exit}' \
            "$THREAT_SCORES_STATE" 2>/dev/null || echo "UNKNOWN")

        if [[ "$current_state" == "CLEAN" ]]; then
            # Check for recent signal
            local last_signal_epoch
            last_signal_epoch=$(awk -F'|' -v s="$source" \
                '$1==s {max=$3} END {print max+0}' \
                "${STATE_DIR}/signal_window.state" 2>/dev/null || echo 0)
            local signal_age=$(( NOW - last_signal_epoch ))

            if [[ $signal_age -gt 60 ]]; then
                _ia_critical "ETHICAL_FLOOR_CLEAN_ENFORCEMENT" \
                    "enforcement on CLEAN source=${source} last_signal=${signal_age}s ago — floor breach"
                (( violations++ )) || true
            fi
        fi

    done < <(awk -F'|' -v ws="$(( NOW - 300 ))" '$1 > ws' "$COURT_EVENTS" 2>/dev/null || true)

    # Floor 3: No JAILED verdict without prior HOUSE_ARREST or QUARANTINED in current session
    while IFS='|' read -r epoch etype source verdict case_id rest; do
        [[ "$etype" == "VERDICT" && "$verdict" == "JAILED" ]] || continue
        [[ $epoch -gt $WINDOW_START ]] || continue

        local had_prior=false
        if [[ -f "$CRIMINAL_LEDGER" ]]; then
            grep -q "^.*|${source}|.*\(HOUSE_ARREST\|QUARANTINED\)" "$CRIMINAL_LEDGER" 2>/dev/null \
                && had_prior=true
        fi

        # Also check court.events for this session
        if grep -qE "^[^|]+\|VERDICT\|${source}\|(HOUSE_ARREST|QUARANTINED)\|" \
            "$COURT_EVENTS" 2>/dev/null; then
            had_prior=true
        fi

        if [[ "$had_prior" == false ]]; then
            _ia_critical "ETHICAL_FLOOR_JAILED_WITHOUT_PRIOR" \
                "JAILED verdict on ${source} with no prior HOUSE_ARREST or QUARANTINED"
            (( violations++ )) || true
        fi

    done < <(grep "|VERDICT|" "$COURT_EVENTS" 2>/dev/null || true)

    # Floor 4: No consent bypass for sovereignty-listed apps
    if [[ -f "${STATE_DIR}/sovereignty.list" && -s "${STATE_DIR}/sovereignty.list" ]]; then
        while IFS='|' read -r sov_source _rest; do
            [[ -z "$sov_source" ]] && continue
            # Check if any recent enforcement skipped consent gate
            if grep -q "${sov_source}|.*CONSENT_NOT_REQUIRED" "$JUDGE_LOG" 2>/dev/null; then
                _ia_critical "ETHICAL_FLOOR_SOVEREIGNTY_BYPASS" \
                    "consent gate bypassed for sovereignty app=${sov_source}"
                (( violations++ )) || true
            fi
        done < "${STATE_DIR}/sovereignty.list"
    fi

    [[ $violations -eq 0 ]] && _ia_pass "ETHICAL_FLOOR" "all floors intact"
}

# ── Run full audit ─────────────────────────────────────────────────────────────

_run_audit() {
    NOW=$(date +%s)
    WINDOW_START=$(( NOW - FLAG_WINDOW ))

    _ia_log "AUDIT_START" "INFO" "cycle=$(date -d @$NOW '+%Y-%m-%d %H:%M:%S')"

    _check_verdict_rate
    _check_score_inflation
    _check_enforcement_evidence
    _check_pipeline_stall
    _check_self_exemption
    _check_ethical_floors

    _ia_log "AUDIT_END" "INFO" "cycle complete"
}

# ── Main ──────────────────────────────────────────────────────────────────────

case "${1:-}" in
    --clear-lock)
        _clear_lock
        exit 0
        ;;
    --status)
        echo "=== Internal Affairs Status ==="
        if [[ -f "$IA_LOCK" ]]; then
            echo "⚠️  PIPELINE SUSPENDED:"
            cat "$IA_LOCK"
        else
            echo "✓  Pipeline running (no lock)"
        fi
        echo ""
        echo "=== Recent Flags (last hour) ==="
        if [[ -f "$IA_LOG" ]]; then
            awk -v ws="$WINDOW_START" -F'|' \
                '$1 ~ /\[IA\]/ && $3 ~ /FLAG|CRITICAL/' "$IA_LOG" | tail -20
        else
            echo "(no log yet)"
        fi
        exit 0
        ;;
    --once)
        _run_audit
        exit 0
        ;;
    --daemon|"")
        _ia_log "DAEMON_START" "INFO" "pid=$$ interval=${AUDIT_INTERVAL}s"
        while true; do
            _run_audit
            sleep "$AUDIT_INTERVAL"
        done
        ;;
    *)
        echo "Usage: internal_affairs.sh [--once|--daemon|--status|--clear-lock]" >&2
        exit 1
        ;;
esac
