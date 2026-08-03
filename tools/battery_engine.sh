#!/data/data/com.termux/files/usr/bin/bash

# -----------------------------
# DEVICE IDENTITY
# -----------------------------

MODEL=$(getprop ro.product.model 2>/dev/null | tr -d '\n')
DEVICE_NAME=$(getprop ro.product.name 2>/dev/null | tr -d '\n')
BRAND=$(getprop ro.product.brand 2>/dev/null | tr -d '\n')
CODENAME=$(getprop ro.product.device 2>/dev/null | tr -d '\n')
ROM=$(getprop ro.build.version.incremental 2>/dev/null | tr -d '\n')

ANDROID=$(getprop ro.build.version.release 2>/dev/null | tr -d '\n')
PLATFORM=$(getprop ro.board.platform 2>/dev/null | tr -d '\n')

# -----------------------------
# ADB / RISH SAFE WRAPPER
# -----------------------------

adb_cmd() {
    if command -v adb >/dev/null 2>&1; then
        adb shell "$@"
    else
        sh -c "$@"
    fi
}

BATTERY_RAW=$(adb_cmd dumpsys battery 2>/dev/null)

LEVEL=$(echo "$BATTERY_RAW" | grep level | awk '{print $2}')
TEMP=$(echo "$BATTERY_RAW" | grep temperature | awk '{print $2}')

# fallback safety
LEVEL=${LEVEL:-0}
TEMP=${TEMP:-0}

TEMP_C=$((TEMP / 10))

STATUS=$(echo "$BATTERY_RAW" | grep status | awk '{print $2}')
STATUS=${STATUS:-unknown}

# -----------------------------
# DEVICE NAME RESOLUTION (Xiaomi-ish)
# -----------------------------

case "$MODEL $CODENAME" in
    *"Redmi 15C"*|*"tapas"*|*"tapash"* )
        FRIENDLY_NAME="Xiaomi Redmi 15C"
        ;;
    *"Note 13 Pro"*|*"garnet"* )
        FRIENDLY_NAME="Redmi Note 13 Pro"
        ;;
    *"Note 12 Pro"*|*"ruby"* )
        FRIENDLY_NAME="Redmi Note 12 Pro"
        ;;
    *)
        FRIENDLY_NAME="$BRAND $MODEL"
        ;;
esac

# -----------------------------
# HARDWARE TIER
# -----------------------------

case "$PLATFORM" in
  mt6768|mt6785|mt67*)
    TIER="MEDIA_TECH_MIDRANGE"
    HEAT_WEIGHT=13   # fixed-point (x10) to avoid float issues
    ;;
  sm8*|sd8*)
    TIER="FLAGSHIP_SNAPDRAGON"
    HEAT_WEIGHT=10
    ;;
  *)
    TIER="UNKNOWN"
    HEAT_WEIGHT=12
    ;;
esac

# -----------------------------
# HEALTH CALC (safe integer math)
# -----------------------------

TEMP_SCORE=$((TEMP_C * HEAT_WEIGHT / 10))
HEALTH_SCORE=$((100 - TEMP_SCORE))

[ "$HEALTH_SCORE" -lt 0 ] && HEALTH_SCORE=0
[ "$HEALTH_SCORE" -gt 100 ] && HEALTH_SCORE=100

if [ "$HEALTH_SCORE" -gt 85 ]; then
  STATE="Excellent"
elif [ "$HEALTH_SCORE" -gt 70 ]; then
  STATE="Good"
elif [ "$HEALTH_SCORE" -gt 50 ]; then
  STATE="Moderate wear"
else
  STATE="Thermal stress"
fi

# -----------------------------
# OUTPUT
# -----------------------------

echo "=============================="
echo " DEVICE ENGINE REPORT"
echo "=============================="
echo "Device: $FRIENDLY_NAME"
echo "Codename: $CODENAME"
echo "Android: $ANDROID"
echo "Platform: $PLATFORM"
echo "Tier: $TIER"
echo "ROM: $ROM"
echo ""
echo "Battery: ${LEVEL}%"
echo "Temp: ${TEMP_C}°C"
echo "Status: $STATUS"
echo ""
echo "Health Score: $HEALTH_SCORE / 100"
echo "State: $STATE"
echo "=============================="
