#!/system/bin/sh
# =========================================================
# MiuiserPeruser: THE GRANITOR PROTOCOL (v2.0)
# Status: Heavy-Duty / Anti-Hydra / GPU-Stabilized
# =========================================================

echo "🚀 Starting the Granitor Protocol..."

# --- PHASE 1: SHIELD THE ALLY ---
# Ensure Termux is treated as a System VIP
echo "🛡️  Protecting Termux..."
am set-standby-bucket com.termux 5
dumpsys deviceidle whitelist +com.termux

# --- PHASE 2: THE MAIN SLAY ---
# Muzzle the primary demons and their shadow cousins
TARGETS="com.miui.securitycenter com.miui.daemon com.lbe.security.miui com.miui.powerkeeper com.xiaomi.joyose com.miui.msa.global com.miui.analytics com.xiaomi.discover com.miui.hybrid com.miui.systemAdSolution com.xiaomi.glgm"

for pkg in $TARGETS; do
    echo "⚔️  Neutralizing $pkg..."
    # Force into the 'Restricted' bucket
    am set-standby-bucket $pkg 45
    # Revoke background execution rights
    appops set $pkg RUN_IN_BACKGROUND ignore
    appops set $pkg RUN_ANY_IN_BACKGROUND ignore
    appops set $pkg START_FOREGROUND ignore
    appops set $pkg WAKE_LOCK ignore
done

# --- PHASE 3: THE BOARD STABILIZER ---
# Fix the "jumping words" and UI jitter
echo "🎨  Stabilizing Graphics & Logic..."
settings put system miui_optimize 0
settings put global use_fixed_refresh_rate 1
settings put system min_refresh_rate 60.0
settings put system peak_refresh_rate 60.0
settings put global hardware_acceleration_optimization false
settings put global low_power 0
settings put global low_power_sticky 0

# --- PHASE 4: FINAL AUDIT ---
echo "🏁  FINAL BOARD AUDIT:"
echo "--------------------------------"
echo -n "Termux Status: " && am get-standby-bucket com.termux
echo -n "Daemon Status: " && appops get com.miui.daemon RUN_IN_BACKGROUND
echo "--------------------------------"
echo "💅 Slay Complete. The highway is clear."
