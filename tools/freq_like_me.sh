#!/data/data/com.termux/files/usr/bin/bash
# ==============================================================================
#  Freq Like Me — CPU Truth Companion
#  Companion to: rocksteadyd daemon
#  Reads: Registry/daemon_results/rocksteadyd.json
#  Theme: Angels with Dirty Faces — dark purples, electric blues, dirty whites
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/rocksteadyd.json"

# ── Colour palette — dark, moody, electric ────────────────────────────────────
P0='\033[38;5;57m'    # deep violet
P1='\033[38;5;63m'    # electric indigo
P2='\033[38;5;69m'    # electric blue
P3='\033[38;5;75m'    # bright blue
P4='\033[38;5;111m'   # ice blue
W0='\033[38;5;255m'   # pure white
W1='\033[38;5;252m'   # dirty white
W2='\033[38;5;246m'   # grey white
DIM='\033[38;5;237m'  # near black
EMBER='\033[38;5;202m' # hot orange — throttle warning
RED='\033[38;5;196m'   # danger red
BOLD='\033[1m'
RESET='\033[0m'

# ── Backend — rish first, adb fallback ───────────────────────────────────────
RISH="$HOME/rish"
ADB="adb -s 127.0.0.1:5555"

run_cmd() {
    if "$RISH" -c "echo ok" 2>/dev/null | grep -q ok; then
        "$RISH" -c "$*" 2>/dev/null
    else
        $ADB shell "$*" 2>/dev/null
    fi
}

read_file() { adb -s 127.0.0.1:5555 shell "cat $1" 2>/dev/null; }

# ── Frequency bar ─────────────────────────────────────────────────────────────
freq_bar() {
    local pct=$1 width=28
    local filled=$(( pct * width / 100 ))
    [ $filled -gt $width ] && filled=$width
    local empty=$(( width - filled ))
    local bar="" i=0

    while [ $i -lt $filled ]; do
        pos=$(( i * 100 / width ))
        if   [ $pos -ge 80 ]; then bar="${bar}${P3}█${RESET}"
        elif [ $pos -ge 60 ]; then bar="${bar}${P2}█${RESET}"
        elif [ $pos -ge 40 ]; then bar="${bar}${P1}█${RESET}"
        else                       bar="${bar}${P0}█${RESET}"
        fi
        i=$((i+1))
    done
    i=0
    while [ $i -lt $empty ]; do
        bar="${bar}${DIM}░${RESET}"
        i=$((i+1))
    done
    echo -n "$bar"
}

# ── Header ────────────────────────────────────────────────────────────────────
header() {
    clear
    echo -e "${P0}╔══════════════════════════════════════════════════════════════════╗${RESET}"
    echo -e "${P1}║${RESET}  ${BOLD}${P3}⚡  F R E Q   L I K E   M E${RESET}${P1}                                    ║${RESET}"
    echo -e "${P2}║${RESET}  ${DIM}CPU truth companion · rocksteadyd daemon${RESET}${P2}                       ║${RESET}"
    echo -e "${P0}╚══════════════════════════════════════════════════════════════════╝${RESET}"
    echo ""
}

# ── Last daemon scan ──────────────────────────────────────────────────────────
show_last_scan() {
    echo -e "${P1}── Last Daemon Scan ──────────────────────────────────────────────${RESET}"

    if [ ! -f "$RESULTS" ]; then
        echo -e "${DIM}  No results yet — rocksteadyd hasn't completed a scan${RESET}"
        echo ""
        return
    fi

    score=$(jq -r '.cpu_score // "?"' "$RESULTS" 2>/dev/null)
    grade=$(jq -r '.grade // "?"' "$RESULTS" 2>/dev/null)
    ts=$(jq -r '.timestamp // "?"' "$RESULTS" 2>/dev/null)
    throttled=$(jq -r '.throttled_cores // 0' "$RESULTS" 2>/dev/null)
    total=$(jq -r '.total_cores // 0' "$RESULTS" 2>/dev/null)
    signals=$(jq -r '.signals_fired // 0' "$RESULTS" 2>/dev/null)
    sys_pct=$(jq -r '.system_cpu_pct // 0' "$RESULTS" 2>/dev/null)

    case "$grade" in
        HEALTHY)    sc="$P4" ;;
        BUSY)       sc="$P2" ;;
        STRESSED)   sc="$EMBER" ;;
        OVERLOADED) sc="$RED" ;;
        *)          sc="$W2" ;;
    esac

    echo -e "  ${W2}Score     :${RESET} ${sc}${BOLD}${score}/100  [${grade}]${RESET}"
    echo -e "  ${W2}Scanned   :${RESET} ${DIM}${ts}${RESET}"
    echo -e "  ${W2}System CPU:${RESET} ${P3}${sys_pct}%${RESET}"
    echo -e "  ${W2}Signals   :${RESET} ${P2}${signals}${RESET}"
    [ "$throttled" -gt 0 ] && \
    echo -e "  ${W2}Throttled :${RESET} ${EMBER}${throttled}${RESET}${W2}/${total} cores${RESET}" || \
    echo -e "  ${W2}Throttled :${RESET} ${P4}none${RESET}"
    echo ""
}

# ── Live CPU frequencies ──────────────────────────────────────────────────────
live_cpu() {
    echo -e "${P2}── Live CPU Frequencies ──────────────────────────────────────────${RESET}"
    printf "  ${W2}%-8s  %-8s  %-8s  %4s  %s${RESET}\n" \
           "Core" "Cur MHz" "Max MHz" "Pct" "Freq"
    echo -e "  ${DIM}────────────────────────────────────────────────────────────${RESET}"

    local eff_total=0 eff_max_t=0 eff_n=0
    local prf_total=0 prf_max_t=0 prf_n=0

    for i in 0 1 2 3 4 5 6 7; do
        cur=$(read_file "/sys/devices/system/cpu/cpu${i}/cpufreq/scaling_cur_freq" 2>/dev/null)
        max=$(read_file "/sys/devices/system/cpu/cpu${i}/cpufreq/cpuinfo_max_freq" 2>/dev/null)
        min=$(read_file "/sys/devices/system/cpu/cpu${i}/cpufreq/cpuinfo_min_freq" 2>/dev/null)
        [ -z "$cur" ] && break

        mhz_cur=$((cur / 1000))
        mhz_max=$((max / 1000))
        pct=$((cur * 100 / max))

        # Cluster by min_freq
        if [ "${min:-0}" -le 600000 ] 2>/dev/null; then
            cluster="${W2}[E]${RESET}"
            eff_total=$((eff_total + cur))
            eff_max_t=$((eff_max_t + max))
            eff_n=$((eff_n + 1))
        else
            cluster="${P3}[P]${RESET}"
            prf_total=$((prf_total + cur))
            prf_max_t=$((prf_max_t + max))
            prf_n=$((prf_n + 1))
        fi

        bar=$(freq_bar $pct)
        flag=""
        [ "$pct" -lt 75 ] && flag=" ${EMBER}throttled${RESET}"
        [ "$pct" -ge 100 ] && flag=" ${P4}${BOLD}MAX${RESET}"

        printf "  %b cpu%-2d  %-8d  %-8d  %3d%%  %b%b\n" \
               "$(echo -e $cluster)" "$i" \
               "$mhz_cur" "$mhz_max" "$pct" "$bar" "$flag"
    done
    echo ""

    # Cluster summary
    if [ $eff_n -gt 0 ] && [ $prf_n -gt 0 ]; then
        eff_pct=$((eff_total * 100 / eff_max_t))
        prf_pct=$((prf_total * 100 / prf_max_t))
        echo -e "${P1}── Cluster Balance ───────────────────────────────────────────────${RESET}"
        echo -e "  ${W2}Efficiency [E]:${RESET} $(freq_bar $eff_pct) ${P2}${eff_pct}%${RESET}"
        echo -e "  ${P3}Performance[P]:${RESET} $(freq_bar $prf_pct) ${P3}${prf_pct}%${RESET}"

        # Imbalance check
        diff=$(( eff_pct - prf_pct ))
        [ $diff -lt 0 ] && diff=$(( -diff ))
        if [ $diff -gt 20 ]; then
            echo -e "  ${EMBER}⚠  Cluster imbalance detected — delta ${diff}%${RESET}"
        else
            echo -e "  ${P4}✓  Clusters balanced${RESET}"
        fi
        echo ""
    fi
}

# ── Top CPU processes via adb ─────────────────────────────────────────────────
top_procs() {
    echo -e "${P1}── Top CPU Consumers ─────────────────────────────────────────────${RESET}"
    local ps_out
    ps_out=$(run_cmd "ps -eo pid,pcpu,args --sort=-pcpu 2>/dev/null | head -12" 2>/dev/null)

    if [ -z "$ps_out" ]; then
        echo -e "${DIM}  ps unavailable${RESET}"
        echo ""
        return
    fi

    echo "$ps_out" | while read -r line; do
        echo -e "  ${W2}${line}${RESET}"
    done
    echo ""
}

# ── System CPU % from /proc/stat ──────────────────────────────────────────────
system_cpu() {
    echo -e "${P2}── System CPU Load ───────────────────────────────────────────────${RESET}"

    # Two samples 1s apart
    read_stat() {
        adb -s 127.0.0.1:5555 shell 'cat /proc/stat' 2>/dev/null | awk '/^cpu / {print $2,$3,$4,$5,$6,$7,$8}'
    }

    s1=$(read_stat); sleep 1; s2=$(read_stat)

    u1=$(echo $s1 | cut -d' ' -f1); n1=$(echo $s1 | cut -d' ' -f2)
    s1v=$(echo $s1 | cut -d' ' -f3); i1=$(echo $s1 | cut -d' ' -f4)

    u2=$(echo $s2 | cut -d' ' -f1); n2=$(echo $s2 | cut -d' ' -f2)
    s2v=$(echo $s2 | cut -d' ' -f3); i2=$(echo $s2 | cut -d' ' -f4)

    total1=$((u1 + n1 + s1v + i1))
    total2=$((u2 + n2 + s2v + i2))
    dtotal=$((total2 - total1))
    didle=$((i2 - i1))

    if [ $dtotal -gt 0 ]; then
        pct=$(( (dtotal - didle) * 100 / dtotal ))
        bar=$(freq_bar $pct)
        echo -e "  System:  ${bar} ${P3}${pct}%${RESET}"
    else
        echo -e "${DIM}  Unable to sample${RESET}"
    fi
    echo ""
}

# ── Action menu ───────────────────────────────────────────────────────────────
menu() {
    echo -e "${P2}── Actions ───────────────────────────────────────────────────────${RESET}"
    echo -e "  ${P4}[1]${RESET} Last rocksteadyd scan results"
    echo -e "  ${P3}[2]${RESET} Live CPU frequencies + cluster balance"
    echo -e "  ${P2}[3]${RESET} Top CPU processes"
    echo -e "  ${P1}[4]${RESET} System CPU load (1s sample)"
    echo -e "  ${P0}[5]${RESET} Full live view (refreshes every 3s)"
    echo -e "  ${P2}[6]${RESET} Force rocksteadyd rescan"
    echo -e "  ${DIM}[q]${RESET} Quit"
    echo ""
    read -p "$(echo -e "${P2}Choice: ${RESET}")" choice

    case "$choice" in
        1) header; show_last_scan ;;
        2) header; live_cpu ;;
        3) header; top_procs ;;
        4) header; system_cpu ;;
        5)
            while true; do
                header
                live_cpu
                system_cpu
                echo -e "${DIM}  Refreshing in 3s — Ctrl+C to stop${RESET}"
                sleep 3
            done
            ;;
        6)
            echo -e "${P2}Triggering rocksteadyd rescan...${RESET}"
            "$BASE/bin/syndicatectl" scan-count rocksteadyd 1 2>/dev/null
            pkill -x rocksteadyd 2>/dev/null
            sleep 1
            "$BASE/bin/rocksteadyd" >> "$BASE/logs/rocksteadyd.log" 2>&1 &
            echo -e "${P3}Daemon restarted — results in ~15s${RESET}"
            sleep 2
            ;;
        q|Q) exit 0 ;;
        *) echo -e "${RED}Invalid choice${RESET}"; sleep 1 ;;
    esac

    menu
}

# ── Main ──────────────────────────────────────────────────────────────────────
header
show_last_scan
menu
