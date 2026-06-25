#!/data/data/com.termux/files/usr/bin/bash
# pcap_ingest.sh — PCAPdroid Export Analyser
# Parses PCAPdroid connection logs and PCAP payloads
# Feeds findings into MiuiserPeruser Registry
# Leith, Patras & Liu (TCD & Univ. Edinburgh, 2021)

BASE="$HOME/MiuiserPeruser"
RESULTS="$BASE/Registry/daemon_results/pcap_ingest.json"
PCAP_DIR="/sdcard/PCAPdroid"

CYN='\033[96m'; YEL='\033[93m'; RED='\033[91m'
WHT='\033[97m'; DIM='\033[2m'; BOLD='\033[1m'; RST='\033[0m'
BGRED='\033[41m'; BGYEL='\033[43m'

# Known Xiaomi telemetry domains
XIAOMI_DOMAINS=(
    "msg.global.xiaomi.net"
    "fr.resolver.msg.global.xiaomi.net"
    "analytics.miui.com"
    "data.mistat.xiaomi.com"
    "tracking.miui.com"
    "sdkconfig.ad.xiaomi.com"
    "api.ad.xiaomi.com"
    "resolver.msg.global.xiaomi.net"
)

clear
printf "${BOLD}${WHT}"
printf "╔══════════════════════════════════════════════════╗\n"
printf "║  📡  PCAP INGEST                                 ║\n"
printf "║  PCAPdroid export analyser                       ║\n"
printf "║  Leith, Patras & Liu (TCD/Edinburgh, 2021)       ║\n"
printf "╚══════════════════════════════════════════════════╝\n"
printf "${RST}\n"

# Find PCAPdroid exports
printf "${CYN}Scanning for PCAPdroid exports...${RST}\n\n"

pcap_files=$(find /sdcard -name "PCAPdroid_*.txt" -o -name "PCAPdroid_*.pcap" 2>/dev/null | sort -r | head -20)
txt_files=$(find /sdcard -name "PCAPdroid_*.txt" 2>/dev/null | sort -r | head -20)

if [ -z "$pcap_files" ] && [ -z "$txt_files" ]; then
    printf "${DIM}No PCAPdroid exports found in /sdcard/${RST}\n"
    printf "${DIM}Export from PCAPdroid: ⋮ → Export → Connections (CSV or text)${RST}\n\n"
    exit 0
fi

printf "${BOLD}${WHT}── DEVICE IDENTIFIER EXTRACTION ───────────────────${RST}\n\n"

device_ids=()
xiaomi_domains=()
total_xiaomi_bytes=0
findings=()

# Parse raw PCAP payloads for device IDs
for f in $txt_files; do
    fname=$(basename "$f")
    printf "${CYN}Analysing: %s${RST}\n" "$fname"
    
    # Extract ASCII strings from hex dump
    strings_out=$(cat "$f" | grep -oP '[a-zA-Z0-9_.@-]{8,}' | grep -v "^[0-9a-f]*$")
    
    # Check for xiaomi.com
    if echo "$strings_out" | grep -q "xiaomi"; then
        domain=$(echo "$strings_out" | grep "xiaomi" | head -1)
        printf "  ${YEL}⚑ Xiaomi domain: %s${RST}\n" "$domain"
        xiaomi_domains+=("$domain")
    fi
    
    # Extract device identifier hashes (40 char hex strings)
    hash=$(echo "$strings_out" | grep -oE '[A-F0-9]{32,}' | head -1)
    if [ -n "$hash" ]; then
        printf "  ${RED}${BOLD}⚠ DEVICE ID HASH: %s${RST}\n" "$hash"
        device_ids+=("$hash")
        findings+=("DEVICE_ID_TRANSMITTED|||${hash}|||${fname}")
    fi

    # Check for CONNH (connection handshake) marker
    if grep -q "CONNH" "$f" 2>/dev/null; then
        printf "  ${RED}⚠ CONNH handshake detected — device registering with Xiaomi${RST}\n"
        findings+=("XIAOMI_HANDSHAKE|||CONNH|||${fname}")
    fi

    # Check for wifi marker
    if grep -q "wifi" "$f" 2>/dev/null; then
        printf "  ${YEL}⚑ WiFi state transmitted${RST}\n"
        findings+=("WIFI_STATE_TRANSMITTED|||wifi|||${fname}")
    fi
    echo ""
done

# Check PCAPdroid CSV exports
csv_files=$(find /sdcard -name "PCAPdroid_*.csv" 2>/dev/null | sort -r | head -5)
if [ -n "$csv_files" ]; then
    printf "${BOLD}${WHT}── CSV CONNECTION LOG ──────────────────────────────${RST}\n\n"
    for f in $csv_files; do
        printf "${CYN}%s${RST}\n" "$(basename $f)"
        # Parse CSV: timestamp,app,proto,remote_host,remote_ip,remote_port,bytes
        tail -n +2 "$f" | while IFS=, read -r ts app proto host ip port bytes rest; do
            host=$(echo "$host" | tr -d '"')
            app=$(echo "$app" | tr -d '"')
            bytes=$(echo "$bytes" | tr -d '"')
            
            # Flag Xiaomi domains
            for domain in "${XIAOMI_DOMAINS[@]}"; do
                if echo "$host" | grep -q "$domain"; then
                    printf "  ${RED}${BOLD}⚠ %-30s → %-40s %s bytes${RST}\n" \
                        "$app" "$host" "$bytes"
                    findings+=("XIAOMI_TELEMETRY|||${host}|||${app}:${bytes}B")
                fi
            done
        done
    done
fi

printf "\n${BOLD}${WHT}── SUMMARY ─────────────────────────────────────────${RST}\n\n"
printf "  ${WHT}Device IDs found:    ${YEL}%d${RST}\n" "${#device_ids[@]}"
printf "  ${WHT}Xiaomi domains:      ${YEL}%d${RST}\n" "${#xiaomi_domains[@]}"
printf "  ${WHT}Total findings:      ${YEL}%d${RST}\n" "${#findings[@]}"

if [ "${#device_ids[@]}" -gt 0 ]; then
    printf "\n  ${RED}${BOLD}⚠ DEVICE IDENTIFIERS BEING TRANSMITTED:${RST}\n"
    for id in "${device_ids[@]}"; do
        printf "    ${RED}%s${RST}\n" "$id"
    done
fi

if [ "${#xiaomi_domains[@]}" -gt 0 ]; then
    printf "\n  ${YEL}⚑ XIAOMI SERVERS CONTACTED:${RST}\n"
    for d in "${xiaomi_domains[@]}"; do
        printf "    ${YEL}%s${RST}\n" "$d"
    done
fi

# Write JSON results
ts=$(date '+%Y-%m-%dT%H:%M:%S')
python3 - << PYEOF
import json
findings_list = []
for f in """${findings[*]}""".split():
    parts = f.split('|||')
    if len(parts) == 3:
        findings_list.append({'type':parts[0],'value':parts[1],'source':parts[2]})

out = {
    'daemon': 'pcap_ingest',
    'version': '1.0',
    'timestamp': '$ts',
    'device_ids_found': ${#device_ids[@]},
    'xiaomi_domains': list(set("""${xiaomi_domains[*]}""".split())),
    'total_findings': ${#findings[@]},
    'findings': findings_list
}
with open('$RESULTS', 'w') as f:
    json.dump(out, f, indent=2)
print("Results saved to Registry")
PYEOF

printf "\n${CYN}Press any key to exit...${RST}\n"
read -n1
