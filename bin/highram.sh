#!/data/data/com.termux/files/usr/bin/bash

GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}Top RAM Users — System + MIUI Intelligence View${NC}"
echo "──────────────────────────────────────────────────────"

# ----------------------------
# 1. KEY MEMORY CONSUMERS
# ----------------------------
echo -e "\n${YELLOW}[1] Key System Memory Consumers${NC}"

adb shell dumpsys meminfo 2>/dev/null | awk '
/Total PSS:/ ||
/system_server/ ||
/com.miui/ ||
/com.android/ ||
/powerkeeper/ ||
/securitycenter/ ||
/analytics/ ||
/cloud/ {
    print
}' | head -n 30

# ----------------------------
# 2. LIVE TOP MEMORY USERS
# ----------------------------
echo -e "\n${YELLOW}[2] Top Processes by RAM${NC}"

adb shell top -b -n 1 -o %MEM 2>/dev/null | head -n 25

# ----------------------------
# 3. RAM SUMMARY
# ----------------------------
echo -e "\n${YELLOW}[3] Memory Summary${NC}"

adb shell free -h 2>/dev/null | awk '/Mem/ {print $3 " used / " $2 " total"}'

# ----------------------------
# 4. MIUI / VENDOR CHECK
# ----------------------------
echo -e "\n${YELLOW}[4] MIUI / Vendor Services${NC}"

adb shell pm list packages 2>/dev/null | grep -E "miui|xiaomi|analytics|daemon|security|msa|cloud" | head -n 25

echo -e "\n${GREEN}Done.${NC}"
