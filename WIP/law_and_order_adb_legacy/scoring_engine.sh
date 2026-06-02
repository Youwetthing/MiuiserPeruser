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

# scoring_engine.sh — Judicial Scoring Engine v2
# Part of MiuiserPeruser Judicial System v2
#
# Called by april_o_neil.sh for each incoming signal.
#
# Usage:
#   scoring_engine.sh <source> <signal> <base_weight> [context_json]
#
# Outputs (stdout, one line):
#   <source>|<new_score>|<verdict_state>|<score_delta>
#
# Side effects:
#   - Updates state/threat_scores.state
#   - Appends to state/signal_window.state
#   - Appends full reasoning to cre/scoring_reasoning.log
#
# Depends on: source_tier_lib.sh (sourced)

set -euo pipefail

# ── Resolve paths ─────────────────────────────────────────────────────────────

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Support running from law_and_order:adb/ directly
if [[ "$SCRIPT_DIR" == */law_and_order:adb ]]; then
    BASE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

STATE_DIR="${BASE_DIR}/state"
CRE_DIR="${BASE_DIR}/cre"
LOG_DIR="${BASE_DIR}/logs"

THREAT_SCORES_STATE="${STATE_DIR}/threat_scores.state"
SIGNAL_WINDOW_STATE="${STATE_DIR}/signal_window.state"
SCORING_LOG="${CRE_DIR}/scoring_reasoning.log"
TURTLECOMD_LOG="${LOG_DIR}/turtlecomd.log"
RAHZERD_LOG="${LOG_DIR}/rahzerd.log"

# ── Source tier library ───────────────────────────────────────────────────────

# shellcheck source=source_tier_lib.sh
source "${SCRIPT_DIR}/source_tier_lib.sh"

# ── Args ──────────────────────────────────────────────────────────────────────

SOURCE="${1:-}"
SIGNAL="${2:-}"
BASE_WEIGHT="${3:-10}"
CONTEXT_JSON="${4:-{}}"

if [[ -z "$SOURCE" || -z "$SIGNAL" ]]; then
    echo "Usage: scoring_engine.sh <source> <signal> <base_weight> [context_json]" >&2
    exit 1
fi

NOW_EPOCH=$(date +%s)

# ── Ensure state files and dirs exist ────────────────────────────────────────

mkdir -p "$STATE_DIR" "$CRE_DIR"
touch "$THREAT_SCORES_STATE" "$SIGNAL_WINDOW_STATE" "$SCORING_LOG"

# ── Locking — simple lockfile, timeout 3s ─────────────────────────────────────

LOCK_FILE="${STATE_DIR}/scoring_engine.lock"
_lock() {
    local deadline=$(( NOW_EPOCH + 3 ))
    while ! mkdir "$LOCK_FILE" 2>/dev/null; do
        [[ $(date +%s) -ge $deadline ]] && { echo "LOCK_TIMEOUT" >&2; exit 1; }
        sleep 0.1
    done
}
_unlock() { rmdir "$LOCK_FILE" 2>/dev/null || true; }
trap _unlock EXIT

_lock

# ── Prune signal_window.state (drop entries older than 120s) ─────────────────

WINDOW_CUTOFF=$(( NOW_EPOCH - 120 ))
if [[ -s "$SIGNAL_WINDOW_STATE" ]]; then
    awk -F'|' -v cutoff="$WINDOW_CUTOFF" '$3 > cutoff' \
        "$SIGNAL_WINDOW_STATE" > "${SIGNAL_WINDOW_STATE}.tmp" \
        && mv "${SIGNAL_WINDOW_STATE}.tmp" "$SIGNAL_WINDOW_STATE"
fi

# ── Load current score for this source ───────────────────────────────────────
# Format: source|current_score|last_updated_epoch|verdict_state|prior_jails|prior_quarantines

CURRENT_SCORE="0"
LAST_UPDATED="0"
VERDICT_STATE="CLEAN"
PRIOR_JAILS="0"
PRIOR_QUARANTINES="0"

if grep -q "^${SOURCE}|" "$THREAT_SCORES_STATE" 2>/dev/null; then
    IFS='|' read -r _src CURRENT_SCORE LAST_UPDATED VERDICT_STATE PRIOR_JAILS PRIOR_QUARANTINES \
        < <(grep "^${SOURCE}|" "$THREAT_SCORES_STATE" | head -1)
PRIOR_JAILS=$(printf "%d" "${PRIOR_JAILS:-0}" 2>/dev/null || echo 0)
PRIOR_QUARANTINES=$(printf "%d" "${PRIOR_QUARANTINES:-0}" 2>/dev/null || echo 0)
fi

# ── Step 1: Base weight is supplied by court_dispatcher ───────────────────────
# Validate it's numeric
if ! [[ "$BASE_WEIGHT" =~ ^[0-9]+(\.[0-9]+)?$ ]]; then
    BASE_WEIGHT=10
fi

# ── Step 2: User Context Gate ─────────────────────────────────────────────────
# Cache file per source, valid for 30s

USER_CONTEXT_CACHE="${STATE_DIR}/.uctx_${SOURCE//[^a-zA-Z0-9._-]/_}"
USER_CONTEXT_MOD="1.00"
USER_CONTEXT_LABEL="background"

if [[ -f "$USER_CONTEXT_CACHE" ]]; then
    cache_age=$(( NOW_EPOCH - $(stat -c %Y "$USER_CONTEXT_CACHE" 2>/dev/null || echo 0) ))
    if [[ $cache_age -lt 30 ]]; then
        IFS='|' read -r USER_CONTEXT_LABEL USER_CONTEXT_MOD < "$USER_CONTEXT_CACHE"
    fi
fi

if [[ "$USER_CONTEXT_LABEL" == "background" && \
      ( ! -f "$USER_CONTEXT_CACHE" || \
        $(( NOW_EPOCH - $(stat -c %Y "$USER_CONTEXT_CACHE" 2>/dev/null || echo 0) )) -ge 30 ) ]]; then

    # Sovereignty override check first
    if is_sovereignty_app "$SOURCE"; then
        USER_CONTEXT_LABEL="sovereignty"
        USER_CONTEXT_MOD="0.15"
    else
        # Check foreground / screen state via superhero_exec
        FG_APP=""
        SCREEN_ON="false"

        if command -v superhero_exec &>/dev/null || [[ -x "${BASE_DIR}/bin/superhero" ]]; then
            FG_RAW=$(timeout 3 "${BASE_DIR}/bin/superhero" exec \
                "dumpsys activity | grep -E 'mResumedActivity|topResumedActivity'" 2>/dev/null || true)
            FG_APP=$(echo "$FG_RAW" | grep -oP '(?<=ActivityRecord\{)[^}]+' | \
                awk '{print $NF}' | cut -d'/' -f1 | head -1 || true)
            SCREEN_RAW=$(timeout 2 "${BASE_DIR}/bin/superhero" exec \
                "dumpsys power | grep 'mWakefulness'" 2>/dev/null || true)
            echo "$SCREEN_RAW" | grep -qi "Awake" && SCREEN_ON="true"
        fi

        if [[ "$FG_APP" == "$SOURCE" && "$SCREEN_ON" == "true" ]]; then
            USER_CONTEXT_LABEL="foreground_active"
            USER_CONTEXT_MOD="0.25"
        elif [[ "$FG_APP" == "$SOURCE" || \
            ( "$SCREEN_ON" == "false" && \
              $(( NOW_EPOCH - LAST_UPDATED )) -lt 300 && \
              "$VERDICT_STATE" != "CLEAN" ) ]]; then
            USER_CONTEXT_LABEL="recent_foreground"
            USER_CONTEXT_MOD="0.50"
        else
            USER_CONTEXT_LABEL="background"
            USER_CONTEXT_MOD="1.00"
        fi
    fi

    echo "${USER_CONTEXT_LABEL}|${USER_CONTEXT_MOD}" > "$USER_CONTEXT_CACHE"
fi

# ── Step 3: Situational Awareness ─────────────────────────────────────────────
# Cache for 60s — read last 60s of turtlecomd.log and rahzerd.log

SITUATIONAL_CACHE="${STATE_DIR}/.sitctx_cache"
SITUATIONAL_MOD="1.00"
SITUATIONAL_LABEL="neutral"

if [[ -f "$SITUATIONAL_CACHE" ]]; then
    cache_age=$(( NOW_EPOCH - $(stat -c %Y "$SITUATIONAL_CACHE" 2>/dev/null || echo 0) ))
    if [[ $cache_age -lt 60 ]]; then
        IFS='|' read -r SIT_HOT_ZONES SIT_ANOMALOUS_SOURCES SIT_NET_DIVERGENCE < "$SITUATIONAL_CACHE"
    else
        # Rebuild cache
        SIT_HOT_ZONES=0
        SIT_ANOMALOUS_SOURCES=0
        SIT_NET_DIVERGENCE=0

        if [[ -f "$TURTLECOMD_LOG" ]]; then
            # Count critical_zones mentions in last 60s of turtlecomd.log
            SIT_HOT_ZONES=$(awk -v cutoff="$(( NOW_EPOCH - 60 ))" \
                '$0 ~ /critical_zones=/ {
                    match($0, /critical_zones=([0-9]+)/, a); 
                    if (a[1]+0 > 0) count++
                } END {print count+0}' "$TURTLECOMD_LOG" 2>/dev/null || echo 0)

            # Count distinct sources with anomalies
            SIT_ANOMALOUS_SOURCES=$(awk \
                '$0 ~ /THERMAL_CRITICAL|THERMAL_WARN/ {
                    match($0, /src=([^ ]+)/, a); sources[a[1]]=1
                } END {print length(sources)+0}' "$TURTLECOMD_LOG" 2>/dev/null || echo 0)
        fi

        if [[ -f "$RAHZERD_LOG" ]]; then
            SIT_NET_DIVERGENCE=$(awk \
                '$0 ~ /divergence=high|NETWORK_ANOMALY/ {count++} 
                 END {print count+0}' "$RAHZERD_LOG" 2>/dev/null || echo 0)
        fi

        echo "${SIT_HOT_ZONES}|${SIT_ANOMALOUS_SOURCES}|${SIT_NET_DIVERGENCE}" > "$SITUATIONAL_CACHE"
    fi
else
    SIT_HOT_ZONES=0; SIT_ANOMALOUS_SOURCES=0; SIT_NET_DIVERGENCE=0
    echo "0|0|0" > "$SITUATIONAL_CACHE"
fi

# Apply situational modifier
case "$SIGNAL" in
    THERMAL_CRITICAL|THERMAL_WARN|CPU_THROTTLING|CPU_HOG|CPU_HOG_CRITICAL)
        if [[ $SIT_HOT_ZONES -ge 8 && $SIT_ANOMALOUS_SOURCES -ge 3 ]]; then
            SITUATIONAL_MOD="0.60"
            SITUATIONAL_LABEL="system_thermal_event"
        elif [[ $SIT_ANOMALOUS_SOURCES -le 1 ]]; then
            SITUATIONAL_MOD="1.30"
            SITUATIONAL_LABEL="isolated_thermal"
        fi
        ;;
    NETWORK_ANOMALY)
        if [[ $SIT_NET_DIVERGENCE -ge 5 ]]; then
            SITUATIONAL_MOD="0.70"
            SITUATIONAL_LABEL="system_net_noise"
        elif [[ $SIT_NET_DIVERGENCE -le 1 ]]; then
            SITUATIONAL_MOD="1.30"
            SITUATIONAL_LABEL="isolated_net"
        fi
        ;;
esac

# ── Step 4: Covariance Matrix ─────────────────────────────────────────────────
# Read signal_window for this source (already pruned to 120s window)

COVARIANCE_COEFF="1.00"
COVARIANCE_LABEL="none"

mapfile -t WINDOW_SIGNALS < <(
    awk -F'|' -v src="$SOURCE" '$1 == src {print $2}' "$SIGNAL_WINDOW_STATE" 2>/dev/null | sort -u
)

# Add current signal to window set for co-occurrence check
WINDOW_SIGNALS+=("$SIGNAL")
mapfile -t WINDOW_SIGNALS < <(printf '%s\n' "${WINDOW_SIGNALS[@]}" | sort -u)

DISTINCT_COUNT="${#WINDOW_SIGNALS[@]}"

# Check covariance rules (highest matching coefficient wins)
_sig_in_window() {
    local sig="$1"
    local s
    for s in "${WINDOW_SIGNALS[@]}"; do
        [[ "$s" == "$sig" ]] && return 0
    done
    return 1
}

if _sig_in_window "INTEGRITY_VIOLATION"; then
    COVARIANCE_COEFF="1.80"
    COVARIANCE_LABEL="INTEGRITY+any"
elif _sig_in_window "NETWORK_ANOMALY" && \
     ( _sig_in_window "CPU_HOG" || _sig_in_window "CPU_HOG_CRITICAL" ); then
    COVARIANCE_COEFF="1.60"
    COVARIANCE_LABEL="NETWORK+CPU_HOG"
elif _sig_in_window "WAKELOCK_ANOMALY" && _sig_in_window "NETWORK_ANOMALY"; then
    COVARIANCE_COEFF="1.50"
    COVARIANCE_LABEL="WAKELOCK+NETWORK"
elif _sig_in_window "THERMAL_CRITICAL" && _sig_in_window "CPU_HOG_CRITICAL"; then
    COVARIANCE_COEFF="1.40"
    COVARIANCE_LABEL="THERMAL_CRIT+CPU_HOG_CRIT"
elif ( _sig_in_window "THERMAL_CRITICAL" || _sig_in_window "THERMAL_WARN" ) && \
       _sig_in_window "NETWORK_ANOMALY"; then
    COVARIANCE_COEFF="1.30"
    COVARIANCE_LABEL="THERMAL+NETWORK"
elif _sig_in_window "CPU_THROTTLING" && _sig_in_window "WAKELOCK_ANOMALY"; then
    COVARIANCE_COEFF="1.20"
    COVARIANCE_LABEL="CPU_THROTTLE+WAKELOCK"
fi

# Stacking bonus: 3+ distinct signal types (always applied on top)
STACK_BONUS="1.00"
if [[ $DISTINCT_COUNT -ge 3 ]]; then
    STACK_BONUS="1.20"
fi

# ── Step 5: Recidivism Multiplier ─────────────────────────────────────────────

RECIDIVISM_MULT="1.00"
RECIDIVISM_LABEL="none"

# Check criminal record directory
CRIMINAL_RECORD_DIR="${BASE_DIR}/state/criminal_record"
LEDGER="${CRIMINAL_RECORD_DIR}/ledger.log"

if [[ -f "$LEDGER" ]]; then
    PRIOR_JAILS_LIVE=$(grep -c "^.*|${SOURCE}|.*JAILED" "$LEDGER" 2>/dev/null | tr -d "\n" | tr -d " " || echo 0)
    PRIOR_QUARANTINES_LIVE=$(grep -c "^.*|${SOURCE}|.*QUARANTINED" "$LEDGER" 2>/dev/null | tr -d "\n" | tr -d " " || echo 0)
else
    PRIOR_JAILS_LIVE="$PRIOR_JAILS"
    PRIOR_QUARANTINES_LIVE="$PRIOR_QUARANTINES"
fi

if [[ $PRIOR_JAILS_LIVE -ge 2 ]]; then
    RECIDIVISM_MULT="2.00"
    RECIDIVISM_LABEL="multi_jail"
elif [[ $PRIOR_JAILS_LIVE -ge 1 ]]; then
    RECIDIVISM_MULT="1.60"
    RECIDIVISM_LABEL="prior_jail"
elif [[ $PRIOR_QUARANTINES_LIVE -ge 1 ]]; then
    RECIDIVISM_MULT="1.30"
    RECIDIVISM_LABEL="prior_quarantine"
fi

# ── Step 6: Source Tier Modifier ──────────────────────────────────────────────

TIER_MOD=$(get_tier_modifier "$SOURCE")
TIER_NAME=$(get_tier_name "$SOURCE")

# ── Compute weighted addition ─────────────────────────────────────────────────
# raw_addition = base_weight × user_ctx × situational × covariance × stack × recidivism × tier

RAW_ADDITION=$(awk \
    -v bw="$BASE_WEIGHT" \
    -v uc="$USER_CONTEXT_MOD" \
    -v sit="$SITUATIONAL_MOD" \
    -v cov="$COVARIANCE_COEFF" \
    -v stk="$STACK_BONUS" \
    -v rec="$RECIDIVISM_MULT" \
    -v tier="$TIER_MOD" \
    'BEGIN { printf "%.4f", bw * uc * sit * cov * stk * rec * tier }')

# New score = current + addition, capped at 100
NEW_SCORE=$(awk \
    -v cur="$CURRENT_SCORE" \
    -v add="$RAW_ADDITION" \
    'BEGIN {
        s = cur + add
        if (s > 100) s = 100
        printf "%.2f", s
    }')

SCORE_DELTA=$(awk \
    -v ns="$NEW_SCORE" \
    -v cs="$CURRENT_SCORE" \
    'BEGIN { printf "%.2f", ns - cs }')

CAP_NOTE=""
FULL_ADDITION=$(awk -v cur="$CURRENT_SCORE" -v add="$RAW_ADDITION" \
    'BEGIN { printf "%.4f", cur + add }')
if awk -v full="$FULL_ADDITION" 'BEGIN { exit (full > 100) ? 0 : 1 }'; then
    CAP_NOTE=" → capped addition=$(awk -v ns="$NEW_SCORE" -v cur="$CURRENT_SCORE" \
        'BEGIN { printf "%.2f", ns - cur }')"
fi

# ── Determine new verdict state ───────────────────────────────────────────────

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

NEW_VERDICT_STATE=$(score_to_state "$NEW_SCORE")

# ── Append signal to window ───────────────────────────────────────────────────

echo "${SOURCE}|${SIGNAL}|${NOW_EPOCH}" >> "$SIGNAL_WINDOW_STATE"

# ── Update threat_scores.state ────────────────────────────────────────────────

# Update prior_jails / prior_quarantines from live ledger counts
NEW_PRIOR_JAILS=$(printf "%d" "${PRIOR_JAILS_LIVE:-0}")
NEW_PRIOR_QUARANTINES=$(printf "%d" "${PRIOR_QUARANTINES_LIVE:-0}")

NEW_ENTRY="${SOURCE}|${NEW_SCORE}|${NOW_EPOCH}|${NEW_VERDICT_STATE}|${NEW_PRIOR_JAILS}|${NEW_PRIOR_QUARANTINES}"

if grep -q "^${SOURCE}|" "$THREAT_SCORES_STATE" 2>/dev/null; then
    # Replace existing entry
    TMP_SCORES="${THREAT_SCORES_STATE}.tmp"
    grep -v "^${SOURCE}|" "$THREAT_SCORES_STATE" > "$TMP_SCORES" || true
    echo "$NEW_ENTRY" >> "$TMP_SCORES"
    mv "$TMP_SCORES" "$THREAT_SCORES_STATE"
else
    echo "$NEW_ENTRY" >> "$THREAT_SCORES_STATE"
fi

# ── Write reasoning log ───────────────────────────────────────────────────────

{
    echo "[SCORING] ${NOW_EPOCH} src=${SOURCE} signal=${SIGNAL} base_weight=${BASE_WEIGHT}"
    echo "  user_context=${USER_CONTEXT_LABEL} modifier=${USER_CONTEXT_MOD}"
    echo "  situational=${SITUATIONAL_LABEL} modifier=${SITUATIONAL_MOD}"
    echo "  covariance=${COVARIANCE_LABEL} coefficient=${COVARIANCE_COEFF} stack_bonus=${STACK_BONUS}"
    echo "  recidivism=${RECIDIVISM_LABEL} multiplier=${RECIDIVISM_MULT}"
    echo "  tier=${TIER_NAME} tier_mod=${TIER_MOD}"
    echo "  raw_addition=${BASE_WEIGHT} × ${USER_CONTEXT_MOD} × ${SITUATIONAL_MOD} × ${COVARIANCE_COEFF} × ${STACK_BONUS} × ${RECIDIVISM_MULT} × ${TIER_MOD} = ${RAW_ADDITION}${CAP_NOTE}"
    echo "  prev_score=${CURRENT_SCORE} score_delta=${SCORE_DELTA} new_score=${NEW_SCORE} state=${NEW_VERDICT_STATE}"
    echo "  window_signals=$(IFS=,; echo "${WINDOW_SIGNALS[*]}") distinct_count=${DISTINCT_COUNT}"
} >> "$SCORING_LOG"

# ── Release lock and emit result ───────────────────────────────────────────────

_unlock

# stdout: caller (april_o_neil.sh) reads this
echo "${SOURCE}|${NEW_SCORE}|${NEW_VERDICT_STATE}|${SCORE_DELTA}"
