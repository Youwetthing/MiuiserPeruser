#!/data/data/com.termux/files/usr/bin/bash

# -------------------------------------------------------------------
#  MiuiserPeruser — Peruse Control
# -------------------------------------------------------------------
#  Full MIUI Permissions Manager & System App Oracle
# -------------------------------------------------------------------

BRAIN="data/daemonhunter_brain.txt"
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; BLUE='\033[0;34m'; CYAN='\033[0;36m'; NC='\033[0m'

# ------------------------------------------------------------
#  COMPLETE KNOWLEDGE BASE
# ------------------------------------------------------------
declare -A PKG_INFO

# Critical System (DO NOT TOUCH)
PKG_INFO["com.miui.home"]="SYSTEM LAUNCHER: Required for home screen, app drawer, and recent apps. DO NOT KILL OR DISABLE — will brick UI."
PKG_INFO["com.miui.securitycore"]="SECURITY CORE: Permission controller, app lock, antivirus engine. Critical for app installs and permissions. DO NOT DISABLE."
PKG_INFO["com.miui.notification"]="NOTIFICATION MANAGER: Required for all system and app notifications. DO NOT KILL."
PKG_INFO["com.miui.system"]="SYSTEM FRAMEWORK: Core MIUI services. DO NOT TOUCH."
PKG_INFO["com.miui.core"]="MIUI SDK CORE: Essential for MIUI features. DO NOT DISABLE."
PKG_INFO["com.miui.permissioncontroller"]="PERMISSION CONTROLLER: Manages runtime permissions. DO NOT DISABLE."

# Safe to Restrict/Starve
PKG_INFO["com.miui.analytics"]="TELEMETRY: Tracks usage and sends data to Xiaomi. Safe to DEEP_STARVE. Revoke all permissions."
PKG_INFO["com.miui.msa.global"]="ADVERTISING ID: Serves ads system-wide. Safe to DEEP_STARVE. Disabling removes ads."
PKG_INFO["com.miui.daemon"]="BACKGROUND TRACKER: Monitors app usage. Safe to DEEP_STARVE. May improve battery."
PKG_INFO["com.xiaomi.joyose"]="TRACKING DAEMON: Background data collection. Safe to STARVE."
PKG_INFO["com.miui.cleanmaster"]="JUNK CLEANER: Aggressive RAM/battery 'optimizer'. Safe to STARVE. Use Android's built-in instead."
PKG_INFO["com.miui.powerkeeper"]="BATTERY MANAGER: Aggressively kills background apps. STARVE improves multitasking."
PKG_INFO["com.miui.cloudservice"]="XIAOMI CLOUD: Backup/sync service. STARVE if you use Google Drive instead."
PKG_INFO["com.miui.cloudbackup"]="CLOUD BACKUP: Background cloud backup. STARVE if not used."
PKG_INFO["com.miui.micloudsync"]="MI CLOUD SYNC: Syncs contacts/photos to Xiaomi. STARVE if not used."
PKG_INFO["com.xiaomi.finddevice"]="FIND DEVICE: Xiaomi's device locator. STARVE if you use Google's Find My Device."
PKG_INFO["com.miui.guardprovider"]="ANTIVIRUS AGGREGATOR: Avast/AVL/Tencent engines. DEEP_STARVE — Google Play Protect is enough."
PKG_INFO["com.miui.securitycenter"]="SECURITY CENTER: Cleaner, game turbo, battery saver. STARVE if not used."
PKG_INFO["com.miui.securitycenter.remote"]="SECURITY CENTER BACKGROUND: Heavy RAM user. STARVE safe."
PKG_INFO["com.miui.securityadd"]="SECURITY ADD-ONS: Additional security features. STARVE safe."
PKG_INFO["com.miui.weather2"]="WEATHER APP: Includes ads and tracking. STARVE if you use another weather app."
PKG_INFO["com.miui.player"]="MUSIC PLAYER: Includes ads. STARVE or UNINSTALL."
PKG_INFO["com.miui.video"]="VIDEO PLAYER: Includes ads. STARVE or UNINSTALL."
PKG_INFO["com.miui.gallery"]="MIUI GALLERY: STARVE if you use Google Photos."
PKG_INFO["com.miui.notes"]="NOTES APP: Requests mic/contacts. RESTRICT permissions or STARVE."
PKG_INFO["com.miui.compass"]="COMPASS: Not essential. STARVE."
PKG_INFO["com.miui.fm"]="FM RADIO: STARVE if not used."
PKG_INFO["com.miui.fmservice"]="FM RADIO BACKEND: Requires RECORD_AUDIO. RESTRICT if you don't use FM."
PKG_INFO["com.miui.screenrecorder"]="SCREEN RECORDER: Requests mic. RESTRICT or STARVE."
PKG_INFO["com.miui.qr"]="QR SCANNER: Requests camera/contacts. RESTRICT permissions."
PKG_INFO["com.miui.aod"]="ALWAYS-ON DISPLAY: Requests location/camera. RESTRICT or STARVE if unused."
PKG_INFO["com.miui.audiomonitor"]="VOICE WAKE-UP: Listens for 'OK Google'. RESTRICT mic if unused."
PKG_INFO["com.miui.misound"]="DOLBY ATMOS: Requests location/mic. RESTRICT if you don't use audio effects."
PKG_INFO["com.miui.backup"]="LOCAL BACKUP: Requests contacts/SMS. RESTRICT if you use Google Backup."
PKG_INFO["com.miui.cotaservice"]="CARRIER OTA: Requests location/contacts. RESTRICT permissions."
PKG_INFO["com.miui.misightservice"]="DIAGNOSTICS: Collects logs/crash reports. DEEP_STARVE safe."
PKG_INFO["com.miui.miwallpaper"]="LIVE WALLPAPER: Respawning RAM hog. DEEP_STARVE (fallback to static)."
PKG_INFO["com.miui.phrase"]="KEYBOARD PHRASES: MIUI keyboard suggestions. STARVE if unused."
PKG_INFO["com.miui.yellowpage"]="ONLINE YELLOW PAGES: In dialer. STARVE safe."
PKG_INFO["com.xiaomi.discover"]="DISCOVER/ADS: Xiaomi's ad platform. DEEP_STARVE."
PKG_INFO["com.xiaomi.xmsf"]="XIAOMI SERVICE FRAMEWORK: Push notifications. DEEP_STARVE if you don't use Mi Cloud."
PKG_INFO["com.xiaomi.xmsfkeeper"]="XMSF KEEPER: Keeps XMSF alive. DEEP_STARVE with XMSF."
PKG_INFO["com.xiaomi.aicr"]="HYPERAI ENGINE: XiaoAi voice assistant components. DEEP_STARVE if unused."
PKG_INFO["com.miui.voiceassist"]="XIAOAI VOICE ASSISTANT: STARVE if you use Google Assistant."
PKG_INFO["com.miui.vsimcore"]="VIRTUAL SIM (eSIM): STARVE if you don't use eSIM."
PKG_INFO["com.xiaomi.account"]="XIAOMI ACCOUNT: Required for Mi Cloud/themes. STARVE if unused."
PKG_INFO["com.xiaomi.bluetooth"]="BLUETOOTH STACK: FREEZE only — don't disable."
PKG_INFO["com.xiaomi.touchservice"]="TOUCHSCREEN SERVICE: Do not touch."
PKG_INFO["com.miui.extraphoto"]="GALLERY ADD-ON: Extra photo effects. STARVE safe."
PKG_INFO["com.miui.mediaeditor"]="PHOTO EDITOR: STARVE if you use Snapseed."
PKG_INFO["com.miui.mediaviewer"]="MEDIA VIEWER: Part of Gallery. STARVE if Gallery disabled."
PKG_INFO["com.miui.miservice"]="XIAOMI SUPPORT: Remote diagnostics. STARVE safe."
PKG_INFO["com.miui.mishare.connectivity"]="MI SHARE: File transfer. STARVE if unused."
PKG_INFO["com.miui.hybrid"]="QUICK APPS: Lightweight app framework. STARVE safe."
PKG_INFO["com.miui.translation"]="TRANSLATION SERVICE: STARVE if unused."
PKG_INFO["com.miui.bugreport"]="BUG REPORTING: STARVE safe."
PKG_INFO["com.miui.calculator"]="CALCULATOR: Harmless. IGNORE or UNINSTALL."
PKG_INFO["com.miui.global.packageinstaller"]="PACKAGE INSTALLER: Required for app installs. DO NOT DISABLE."

echo "=============================================================================="
echo -e "              \( {CYAN}🔐 Peruse Control — MIUI System App Oracle \){NC}"
echo "=============================================================================="
echo "Read each panel before taking action."
echo -e "  \( {RED}⛔ CRITICAL \){NC} — Do NOT touch. Will break your phone."
echo -e "  \( {YELLOW}⚠️  CAUTION \){NC} — Can be restricted. May affect related features."
echo -e "  \( {GREEN}✅ SAFE \){NC} — Safe to starve, restrict, or deep starve."
echo ""

# Load brain
declare -A seen
while IFS='|' read -r pkg action extra; do
    [[ -z "\( pkg" || " \){pkg:0:1}" == "#" ]] && continue
    seen["$pkg"]=1
done < "$BRAIN"

while read -r line; do
    [[ -z "\( line" || " \){line:0:1}" == "#" || "$line" == *"|"* ]] && continue
    seen["$line"]=1
done < "$BRAIN"

# Display
for pkg in "${!seen[@]}"; do
    info="${PKG_INFO[$pkg]}"
    if [[ -z "$info" ]]; then
        info="No description available. Treat with CAUTION."
    fi

    if [[ "$info" == *"DO NOT"* ]] || [[ "$info" == *"CRITICAL"* ]] || [[ "$info" == *"brick"* ]]; then
        color="$RED"
        risk="⛔ CRITICAL"
    elif [[ "$info" == *"RESTRICT"* ]] || [[ "$info" == *"CAUTION"* ]] || [[ "$info" == *"may affect"* ]]; then
        color="$YELLOW"
        risk="⚠️  CAUTION"
    else
        color="$GREEN"
        risk="✅ SAFE"
    fi

    echo -e "\( {color}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ \){NC}"
    echo -e "\( {color}[ \){risk}]${NC}  ${BLUE}\( pkg \){NC}"
    echo -e "${color}   📋 \( info \){NC}"
    
    if [[ "$info" == *"DO NOT"* ]] || [[ "$info" == *"brick"* ]]; then
        echo -e "   \( {RED}⛔ ADVICE: DO NOT KILL OR STARVE. IGNORE THIS PACKAGE. \){NC}"
    elif [[ "$info" == *"DEEP_STARVE"* ]]; then
        echo -e "   \( {GREEN}→ RECOMMENDED: [d]eep starve (suspend + disable + appops + OOM) \){NC}"
    elif [[ "$info" == *"STARVE"* ]]; then
        echo -e "   \( {GREEN}→ RECOMMENDED: [s]tarve (disable) \){NC}"
    elif [[ "$info" == *"RESTRICT"* ]]; then
        echo -e "   \( {YELLOW}→ RECOMMENDED: [r]estrict permissions (revoke dangerous perms) \){NC}"
    elif [[ "$info" == *"FREEZE"* ]]; then
        echo -e "   \( {YELLOW}→ RECOMMENDED: [f]reeze (suspend only — keep available) \){NC}"
    else
        echo -e "   \( {CYAN}→ RECOMMENDED: Review manually. IGNORE if unsure. \){NC}"
    fi
    echo ""
done

echo -e "\( {CYAN}============================================================================== \){NC}"
echo "To apply any of these recommendations, use the RAM Slammer watch script:"
echo "   ./bin/watch_ram_slammers.sh"
echo ""
read -n 1 -s -p "Press any key to return..."
