#!/data/data/com.termux/files/usr/bin/bash
# dialer_spy.sh — Call Event Telemetry Monitor
# Watches for call state changes and timestamps them
# Based on Leith et al. (TCD/Edinburgh 2021) finding that
# Xiaomi logs call start/end timestamps to remote servers.

BASE="$HOME/MiuiserPeruser"
ADB="/data/data/com.termux/files/home/.cargo/bin/adb_cli"
RESULTS="$BASE/Registry/daemon_results/dialer_spy.json"
LOG="$BASE/logs/dialer_spy.log"

RED='\033[41m\033[97m'; YEL='\033[43m\033[30m'
CYN='\033[96m'; WHT='\033[97m'; DIM='\033[2m'
BOLD='\033[1m'; RST='\033[0m'

mkdir -p "$BASE/logs"

adb_run() { $ADB tcp 127.0.0.1:5555 shell "$1" 2>/dev/null; }

get_call_state() {
    adb_run "dumpsys telephony.registry 2>/dev/null | grep 'mCallState=' | head -1" \
        | grep -oE 'mCallState=[0-9]+' | cut -d= -f2
}

get_incoming_number() {
    adb_run "dumpsys telephony.registry 2>/dev/null | grep 'mCallIncomingNumber=' | head -1" \
        | grep -oE 'mCallIncomingNumber=.*' | cut -d= -f2 | tr -d ' '
}

get_null_bytes() {
    # Check if nulld caught transmission during call
    local f="$BASE/Registry/daemon_results/nulld.json"
    [ -f "$f" ] && jq -r '.traffic.idle_tx_delta // 0' "$f" 2>/dev/null || echo 0
}

state_name() {
    case "$1" in
        0) echo "IDLE" ;;
        1) echo "RINGING" ;;
        2) echo "OFFHOOK" ;;
        *) echo "UNKNOWN" ;;
    esac
}

clear
printf "${BOLD}${WHT}"
printf "╔══════════════════════════════════════════════════╗\n"
printf "║  📞  DIALER SPY                                  ║\n"
printf "║  Call event telemetry monitor                    ║\n"
printf "║  Based on Leith et al. TCD/Edinburgh 2021        ║\n"
printf "╚══════════════════════════════════════════════════╝\n"
printf "${RST}\n"

printf "${CYN}Monitoring call state — press Ctrl+C to stop${RST}\n\n"
printf "${DIM}Leith finding: Xiaomi logs call timestamps to remote servers.${RST}\n"
printf "${DIM}This tool catches those state transitions from inside the device.${RST}\n\n"

printf " ${YEL}[1]${RST} ${WHT}Live call monitor${RST}\n"
printf " ${YEL}[2]${RST} ${WHT}Call history + service audit${RST}\n"
printf " ${YEL}[q]${RST} ${WHT}Quit${RST}\n\n"
read -r -p "$(printf " ${CYN}Choice: ${RST}")" _choice
case "$_choice" in
    2) dump_historical; exit 0 ;;
    q|Q) exit 0 ;;
esac

prev_state=""
call_start=""
call_events=()
event_count=0


dump_historical() {
    printf "\n${BOLD}${CYN}── Historical Call Events ─────────────────────────────────${RST}\n"
    local raw
    raw=$(adb_run "dumpsys telecom 2>/dev/null | grep -E 'startTime|endTime|callTerminationReason|MISSED|REJECTED|ANSWERED|BankScam'")

    printf "${DIM}Checking for Google scam detection service...${RST}\n"
    local scam
    scam=$(adb_run "dumpsys telecom 2>/dev/null | grep -i BankScam")
    if [ -n "$scam" ]; then
        printf "${YEL}${BOLD} ⚠ GOOGLE SCAM DETECTION ACTIVE ON CALLS ${RST}\n"
        printf "${WHT}  com.google.android.gms/.bankscamwarning.services.BankScamCallDetectionService${RST}\n"
        printf "${DIM}  Google listens to all calls for scam detection.${RST}\n\n"
    fi

    printf "${CYN}Recent call termination events:${RST}\n"
    echo "$raw" | grep -E "callTerminationReason|endTime" | while IFS= read -r line; do
        if echo "$line" | grep -q "endTime"; then
            local epoch_ms=$(echo "$line" | grep -oE '[0-9]{13}')
            if [ -n "$epoch_ms" ]; then
                local ts_human=$(date -d "@$((epoch_ms/1000))" '+%Y-%m-%d %H:%M:%S' 2>/dev/null)
                printf "  ${DIM}End: %s${RST}\n" "${ts_human:-$epoch_ms}"
            fi
        fi
        if echo "$line" | grep -q "callTerminationReason"; then
            local reason=$(echo "$line" | grep -oE 'Code: \([^)]+\)' | head -1)
            printf "  ${WHT}Reason: %s${RST}\n" "$reason"
        fi
    done
    printf "\n"
}

while true; do
    state=$(get_call_state)
    ts=$(date '+%Y-%m-%dT%H:%M:%S')
    ts_human=$(date '+%H:%M:%S')

    if [ "$state" != "$prev_state" ] && [ -n "$state" ]; then
        state_str=$(state_name "$state")
        prev_str=$(state_name "$prev_state")

        case "$state" in
            1) # RINGING
                number=$(get_incoming_number)
                [ -z "$number" ] && number="unknown"
                printf "${YEL}${BOLD} INCOMING CALL ${RST} ${WHT}%s${RST}  ${DIM}%s${RST}\n" \
                    "$ts_human" "$number"
                echo "[$ts] RINGING number=$number" >> "$LOG"
                call_events+=("${ts}|||RINGING|||${number}")
                event_count=$((event_count+1))
                ;;
            2) # OFFHOOK
                call_start="$ts"
                call_start_epoch=$(date +%s)
                printf "${RED}${BOLD} CALL CONNECTED ${RST} ${WHT}%s${RST}\n" "$ts_human"
                echo "[$ts] OFFHOOK — call connected" >> "$LOG"
                call_events+=("${ts}|||OFFHOOK|||connected")
                event_count=$((event_count+1))
                ;;
            0) # IDLE
                if [ "$prev_state" = "2" ] && [ -n "$call_start" ]; then
                    end_epoch=$(date +%s)
                    start_epoch=$(date -d "$call_start" +%s 2>/dev/null || echo $end_epoch)
                    duration=$((end_epoch - start_epoch))
                    printf "${CYN}${BOLD} CALL ENDED ${RST} ${WHT}%s${RST}  duration: ${YEL}%ds${RST}\n" \
                        "$ts_human" "$duration"
                    echo "[$ts] IDLE — call ended duration=${duration}s" >> "$LOG"
                    call_events+=("${ts}|||ENDED|||${duration}s")
                    event_count=$((event_count+1))

                    # Check for suspicious transmission during call
                    null_tx=$(get_null_bytes)
                    if [ "${null_tx:-0}" -gt 1024 ]; then
                        printf "${RED}${BOLD} ⚠ DATA TRANSMITTED DURING CALL: %dKB ${RST}\n" \
                            "$((null_tx/1024))"
                        echo "[$ts] SUSPICIOUS: ${null_tx}B transmitted during call" >> "$LOG"
                    fi
                    call_start=""
                elif [ "$prev_state" = "1" ]; then
                    printf "${DIM} MISSED/REJECTED  %s${RST}\n" "$ts_human"
                    echo "[$ts] IDLE — missed/rejected" >> "$LOG"
                    call_events+=("${ts}|||MISSED|||")
                    event_count=$((event_count+1))
                fi
                ;;
        esac

        # Write JSON
        python3 - << PYEOF
import json, time
events = []
for e in """${call_events[*]}""".split():
    parts = e.split('|||')
    if len(parts) == 3:
        events.append({'timestamp':parts[0],'event':parts[1],'detail':parts[2]})
out = {
    'daemon': 'dialer_spy',
    'version': '1.0',
    'timestamp': '$ts',
    'total_events': $event_count,
    'current_state': '$state_str',
    'events': events
}
with open('$RESULTS', 'w') as f:
    json.dump(out, f, indent=2)
PYEOF

        prev_state="$state"
    fi

    # Status line
    printf "\r${DIM}state: %-10s  events: %d  %s${RST}" \
        "$(state_name $state)" "$event_count" "$ts_human"

    sleep 2
done
