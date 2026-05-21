#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}

# parole_engine.sh — Parole Engine Daemon v2
# Part of MiuiserPeruser Judicial System v2
#
# Runs as a daemon. Loop every 30 seconds.
#
# Usage:
#   parole_engine.sh [--once|--daemon|--status|--grant <source> [duration_mins]]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

STATE_DIR="${BASE_DIR}/state"
CRE_DIR="${BASE_DIR}/cre"

THREAT_SCORES_STATE="${STATE_DIR}/threat_scores.state"
SIGNAL_WINDOW_STATE="${STATE_DIR}/signal_window.state"
COURT_EVENTS="${STATE_DIR}/court.events"
COURT_EVENTS_LOCK="${STATE_DIR}/court.events.lock"
PASS_REGISTRY="${STATE_DIR}/visitors_pass/pass_registry"
PAROLE_LOG="${CRE_DIR}/parole_engine.log"
IA_LOCK="${STATE_DIR}/internal_affairs.lock"

DECAY_FACTOR="0.88"
DECAY_INTERVAL=120
ACTIVE_SIGNAL_WINDOW=60

# ── PID management ────────────────────────────────────────────────────────────

PID_FILE="${BASE_DIR}/pipes/pids/parole_engine.pid"
mkdir -p "$(dirname "$PID_FILE")" "$STATE_DIR" "$CRE_DIR"
mkdir -p "$(dirname "$PASS_REGISTRY")"
touch "$THREAT_SCORES_STATE" "$SIGNAL_WINDOW_STATE" "$PAROLE_LOG"


# ── Logging ───────────────────────────────────────────────────────────────────

_log() {
    echo "[PAROLE] $(date +%s) $*" >> "$PAROLE_LOG"
}

# ── Court event emitter ───────────────────────────────────────────────────────

_emit_court_event() {
    local event="$1"
    (
        flock -x 200
        echo "$event" >> "$COURT_EVENTS"
    ) 200>"$COURT_EVENTS_LOCK"
}

# ── Score helpers ─────────────────────────────────────────────────────────────

score_to_state() {
    local score="$1"
    awk -v s="$score" 'BEGIN {
        if (s >= 80) print "JAILED"
        else if (s >= 70) print "HOUSE_ARREST"
        else if (s >= 50) print "QUARANTINED"
        else if (s >= 40) print "WARNED"
        else if (s >= 20) print "WATCHED"
        else print "CLEAN"
    }'
}

state_rank() {
    case "$1" in
        JAILED)       echo 5 ;;
        HOUSE_ARREST) echo 4 ;;
        QUARANTINED)  echo 3 ;;
        WARNED)       echo 2 ;;
        WATCHED)      echo 1 ;;
        CLEAN)        echo 0 ;;
        *)            echo 0 ;;
    esac
}

# ── Locking ───────────────────────────────────────────────────────────────────

LOCK_FILE="${STATE_DIR}/scoring_engine.lock"

_lock() {
    local deadline=$(( $(date +%s) + 5 ))
    while ! mkdir "$LOCK_FILE" 2>/dev/null; do
        if [[ $(date +%s) -ge $deadline ]]; then
            _log "LOCK_TIMEOUT skipping tick"
            return 1
        fi
        sleep 0.2
    done
    return 0
}

_unlock() {
    rmdir "$LOCK_FILE" 2>/dev/null || true
}

# ── Decay tick ────────────────────────────────────────────────────────────────

_apply_decay_tick() {
    local NOW
    NOW=$(date +%s)
    local ACTIVE_CUTOFF=$(( NOW - ACTIVE_SIGNAL_WINDOW ))

    if [[ ! -s "$THREAT_SCORES_STATE" ]]; then
        return 0
    fi

    if ! _lock; then
        return 0
    fi

    local tmp_scores="${STATE_DIR}/.threat_scores.tmp.$$"

    local src score last_updated state prior_jails prior_quarantines
    while IFS='|' read -r src score last_updated state prior_jails prior_quarantines; do
        [[ -z "$src" ]] && continue

        local had_recent_signal=false
        if awk -F'|' -v s="$src" -v cutoff="$ACTIVE_CUTOFF" \
            '$1 == s && $3 > cutoff {found=1; exit} END {exit !found}' \
            "$SIGNAL_WINDOW_STATE" 2>/dev/null; then
            had_recent_signal=true
        fi

        if [[ "$had_recent_signal" == true ]]; then
            echo "${src}|${score}|${last_updated}|${state}|${prior_jails}|${prior_quarantines}" \
                >> "$tmp_scores"
            _log "DECAY_SKIP src=${src} score=${score} reason=active_signal_window"
            continue
        fi

        if awk -v s="$score" 'BEGIN { exit (s < 1) ? 0 : 1 }'; then
            echo "${src}|0.00|${last_updated}|CLEAN|${prior_jails}|${prior_quarantines}" \
                >> "$tmp_scores"
            continue
        fi

        local new_score
        new_score=$(awk -v s="$score" -v f="$DECAY_FACTOR" \
            'BEGIN { v = s * f; if (v < 0) v = 0; printf "%.2f", v }')

        local new_state
        new_state=$(score_to_state "$new_score")

        local old_rank new_rank
        old_rank=$(state_rank "$state")
        new_rank=$(state_rank "$new_state")

        if [[ $new_rank -lt $old_rank ]]; then
            _log "DE-ESCALATE src=${src} ${state}→${new_state} score ${score}→${new_score}"
            _emit_court_event "$(date +%s)|PAROLE|${src}|DEESCALATE|${state}→${new_state}|score=${new_score}"
        fi

        echo "${src}|${new_score}|${last_updated}|${new_state}|${prior_jails}|${prior_quarantines}" \
            >> "$tmp_scores"

    done < "$THREAT_SCORES_STATE"

    if [[ -f "$tmp_scores" ]]; then
        mv "$tmp_scores" "$THREAT_SCORES_STATE"
    fi

    _unlock
    return 0
}

# ── Visitors pass processing ──────────────────────────────────────────────────

_process_visitors_passes() {
    local NOW
    NOW=$(date +%s)

    if [[ ! -f "$PASS_REGISTRY" || ! -s "$PASS_REGISTRY" ]]; then
        return 0
    fi

    local tmp_passes="${STATE_DIR}/.pass_registry.tmp.$$"

    local src granted_state parole_state grant_epoch expiry_epoch duration_mins
    while IFS='|' read -r src granted_state parole_state grant_epoch expiry_epoch duration_mins; do
        [[ -z "$src" ]] && continue

        if [[ $NOW -ge $expiry_epoch ]]; then
            _log "PASS_EXPIRED src=${src} parole_was=${parole_state} re-contain=${granted_state}"
            _emit_court_event "$(date +%s)|PAROLE|${src}|PASS_EXPIRED|${parole_state}→${granted_state}|expired=${expiry_epoch}"

            if grep -q "^${src}|" "$THREAT_SCORES_STATE" 2>/dev/null; then
                local cur_score
                cur_score=$(awk -F'|' -v s="$src" '$1==s {print $2; exit}' "$THREAT_SCORES_STATE")

                local natural_state
                natural_state=$(score_to_state "$cur_score")
                local natural_rank granted_rank
                natural_rank=$(state_rank "$natural_state")
                granted_rank=$(state_rank "$granted_state")

                if [[ $natural_rank -gt $granted_rank ]]; then
                    local floor_score
                    case "$granted_state" in
                        JAILED)       floor_score=80 ;;
                        HOUSE_ARREST) floor_score=70 ;;
                        QUARANTINED)  floor_score=50 ;;
                        WARNED)       floor_score=40 ;;
                        WATCHED)      floor_score=20 ;;
                        *)            floor_score=0  ;;
                    esac
                    local tmp_s="${STATE_DIR}/.threat_scores_pass.tmp.$$"
                    awk -F'|' -v s="$src" -v fs="$floor_score" -v gs="$granted_state" -v now="$NOW" \
                        'OFS="|" { if ($1==s) { $2=fs; $3=now; $4=gs } print }' \
                        "$THREAT_SCORES_STATE" > "$tmp_s"
                    mv "$tmp_s" "$THREAT_SCORES_STATE"
                else
                    _log "PASS_EXPIRED_NATURAL_CLEAN src=${src} natural_state=${natural_state}"
                fi
            fi
        else
            echo "${src}|${granted_state}|${parole_state}|${grant_epoch}|${expiry_epoch}|${duration_mins}" \
                >> "$tmp_passes"
        fi

    done < "$PASS_REGISTRY"

    if [[ -f "$tmp_passes" ]]; then
        mv "$tmp_passes" "$PASS_REGISTRY"
    else
        > "$PASS_REGISTRY"
    fi

    return 0
}

# ── Grant a visitors pass ─────────────────────────────────────────────────────

_grant_pass() {
    local src="$1"
    local duration_mins="${2:-30}"
    local NOW
    NOW=$(date +%s)

    if ! grep -q "^${src}|" "$THREAT_SCORES_STATE" 2>/dev/null; then
        echo "ERROR: source ${src} not in threat_scores.state" >&2
        return 1
    fi

    local score state _s _lu prior_jails prior_quarantines
    IFS='|' read -r _s score _lu state prior_jails prior_quarantines \
        < <(grep "^${src}|" "$THREAT_SCORES_STATE" | head -1)

    local parole_state
    case "$state" in
        JAILED)       parole_state="HOUSE_ARREST" ;;
        HOUSE_ARREST) parole_state="QUARANTINED"  ;;
        *)
            echo "ERROR: ${src} is ${state} — parole only from JAILED or HOUSE_ARREST" >&2
            return 1
            ;;
    esac

    local expiry=$(( NOW + duration_mins * 60 ))

    if [[ -f "$PASS_REGISTRY" ]]; then
        grep -v "^${src}|" "$PASS_REGISTRY" > "${PASS_REGISTRY}.tmp" 2>/dev/null || true
        mv "${PASS_REGISTRY}.tmp" "$PASS_REGISTRY"
    fi

    echo "${src}|${state}|${parole_state}|${NOW}|${expiry}|${duration_mins}" >> "$PASS_REGISTRY"

    local tmp_s="${STATE_DIR}/.threat_scores_grant.tmp.$$"
    awk -F'|' -v s="$src" -v ps="$parole_state" -v now="$NOW" \
        'OFS="|" { if ($1==s) $4=ps; print }' \
        "$THREAT_SCORES_STATE" > "$tmp_s"
    mv "$tmp_s" "$THREAT_SCORES_STATE"

    _log "PASS_GRANTED src=${src} from=${state} parole_to=${parole_state} duration=${duration_mins}min expires=${expiry}"
    _emit_court_event "${NOW}|PAROLE|${src}|PASS_GRANTED|${state}→${parole_state}|duration=${duration_mins}min"

    echo "Visitors pass granted: ${src} → ${parole_state} for ${duration_mins} minutes"
    return 0
}

# ── IA lock check ─────────────────────────────────────────────────────────────

_check_ia_lock() {
    if [[ -f "$IA_LOCK" ]]; then
        _log "SUSPENDED by internal_affairs.lock — skipping decay tick"
        return 1
    fi
    return 0
}

# ── Main ──────────────────────────────────────────────────────────────────────

case "${1:-}" in
    --grant)
        shift
        _grant_pass "$@"
        exit $?
        ;;
    --once)
        if _check_ia_lock; then
            _apply_decay_tick
            _process_visitors_passes
        fi
        exit 0
        ;;
    --status)
        echo "=== Threat Scores ==="
        if [[ -s "$THREAT_SCORES_STATE" ]]; then
            column -t -s'|' -N "SOURCE,SCORE,LAST_SEEN,STATE,PRIOR_JAILS,PRIOR_Q" \
                "$THREAT_SCORES_STATE"
        else
            echo "(empty)"
        fi
        echo ""
        echo "=== Active Visitors Passes ==="
        if [[ -f "$PASS_REGISTRY" && -s "$PASS_REGISTRY" ]]; then
            column -t -s'|' -N "SOURCE,GRANTED_FROM,PAROLE_TO,GRANTED,EXPIRES,DURATION_MIN" \
                "$PASS_REGISTRY"
        else
            echo "(none active)"
        fi
        exit 0
        ;;
    --daemon|"")
        : # _setup_daemon_pid disabled
        _log "START decay_interval=${DECAY_INTERVAL}s decay_factor=${DECAY_FACTOR}"
        while true; do
            if _check_ia_lock; then
                _apply_decay_tick  || true
                _process_visitors_passes || true
            fi
            sleep "$DECAY_INTERVAL"
        done
        ;;
    *)
        echo "Usage: parole_engine.sh [--once|--daemon|--status|--grant <source> [duration_mins]]" >&2
        exit 1
        ;;
esac
