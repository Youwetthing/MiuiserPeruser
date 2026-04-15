#!/data/data/com.termux/files/usr/bin/bash
# MiuiserPeruser — Daemon Hunter (self-learning MIUI killer)

BRAIN="data/daemonhunter_brain.txt"
LOG="logs/daemonhunter.log"

mkdir -p data
touch "$BRAIN" 2>/dev/null

echo "[$(date)] Daemon Hunter started — self-learning brain active" >> "$LOG"

MIUI_BLOAT_BASE=(
    "com.miui.analytics"
    "com.miui.msa.global"
    "com.miui.powerkeeper"
    "com.miui.daemon"
    "com.miui.cleanmaster"
    "com.miui.cloudservice"
    "com.miui.cloud"
    "com.miui.systemAdSolution"
    "com.xiaomi.finddevice"
    "com.miui.securitycenter"
    "com.miui.securityadd"
    "com.miui.guardprovider"
)

while true; do
    echo "[$(date)] Daemon Hunter round starting..." >> "$LOG"

    # Load learned processes from brain
    mapfile -t learned < "$BRAIN" 2>/dev/null

    ALL_TARGETS=("\( {MIUI_BLOAT_BASE[@]}" " \){learned[@]}")

    for pkg in "${ALL_TARGETS[@]}"; do
        if pm list packages | grep -q "$pkg"; then
            am force-stop "$pkg" 2>/dev/null
            killall -9 "$pkg" 2>/dev/null
            echo "[$(date)] Killed $pkg" >> "$LOG"
            # Learn it if not already in brain
            if ! grep -q "^\( pkg \)" "$BRAIN" 2>/dev/null; then
                echo "$pkg" >> "$BRAIN"
                echo "[$(date)] Learned new target: $pkg" >> "$LOG"
            fi
        fi
    done

    echo "[$(date)] Round complete — next in 40 seconds" >> "$LOG"
    sleep 40
done
