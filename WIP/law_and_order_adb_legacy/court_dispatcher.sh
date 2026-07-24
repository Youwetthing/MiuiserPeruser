#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../../lib/miuiserperuser_common.sh"

# court_dispatcher.sh — Court Dispatcher v2
# Part of MiuiserPeruser Judicial System v2
#
# Reads from: judgement.pipe  (written to by daemon fleet)
# Writes to:  ingest.pipe (april_o_neil reads this)
# Wire format: SOURCE|SIGNAL|WEIGHT|CONTEXT

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
    echo "[DISPATCHER] $(date +%s) $*" >> "$LOG_FILE"
}

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
        *)                     echo 8  ;;
    esac
}

_log "START listening on ${JUDGEMENT_PIPE}"

while true; do
    while IFS='|' read -r SOURCE SIGNAL CONTEXT; do
        [[ -z "$SOURCE" || -z "$SIGNAL" ]] && continue

        if [[ -f "$IA_LOCK" ]]; then
            _log "SUSPENDED src=${SOURCE} signal=${SIGNAL} — pipeline locked"
            continue
        fi

        WEIGHT=$(_signal_to_weight "$SIGNAL")
        _log "DISPATCH src=${SOURCE} signal=${SIGNAL} weight=${WEIGHT}"
        echo "${SOURCE}|${SIGNAL}|${WEIGHT}|${CONTEXT:-}" > "$EXECUTION_PIPE"

    done < "$JUDGEMENT_PIPE"
done
