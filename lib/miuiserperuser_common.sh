# MiuiserPeruser Common Library

# ------------------------------------------------------------
#  Unified Shell Dispatcher (ShizukuHelper > ADB > rish)
# ------------------------------------------------------------
run_shell() {
    # 1. ShizukuHelper (most stable for Shizuku)
    if [ -f "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" ]; then
        output=$(java -jar "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" "$@" 2>/dev/null)
        if [ $? -eq 0 ] && [ -n "$output" ]; then
            echo "$output"
            return 0
        fi
    fi

    # 2. ADB (reliable when USB connected)
    if command -v adb >/dev/null 2>&1 && adb shell echo ready 2>/dev/null | grep -q ready; then
        adb shell "$@" 2>/dev/null
        return 0
    fi

    # 3. rish (fallback)
    if [ -x "$HOME/.shizuku/rish" ] && "$HOME/.shizuku/rish" -c "echo ready" 2>/dev/null | grep -q ready; then
        "$HOME/.shizuku/rish" -c "$*" 2>/dev/null
        return 0
    fi

    return 1
}

# ------------------------------------------------------------
#  Unified Shell Dispatcher (ShizukuHelper > ADB > rish)
# ------------------------------------------------------------
run_shell() {
    # 1. ShizukuHelper (most stable for Shizuku)
    if [ -f "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" ]; then
        output=$(java -jar "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" "$@" 2>/dev/null)
        if [ $? -eq 0 ] && [ -n "$output" ]; then
            echo "$output"
            return 0
        fi
    fi

    # 2. ADB (reliable when USB connected)
    if command -v adb >/dev/null 2>&1 && adb shell echo ready 2>/dev/null | grep -q ready; then
        adb shell "$@" 2>/dev/null
        return 0
    fi

    # 3. rish (fallback)
    if [ -x "$HOME/.shizuku/rish" ] && "$HOME/.shizuku/rish" -c "echo ready" 2>/dev/null | grep -q ready; then
        "$HOME/.shizuku/rish" -c "$*" 2>/dev/null
        return 0
    fi

    return 1
}
