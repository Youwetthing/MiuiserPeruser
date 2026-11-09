# MiuiserPeruser Common Library

# ------------------------------------------------------------
#  Unified Shell Dispatcher (ADB > rish)
# ------------------------------------------------------------
run_shell() {
    # 1. ADB (reliable when USB connected)
    if command -v adb >/dev/null 2>&1 && adb shell echo ready 2>/dev/null | grep -q ready; then
        adb shell "$@" 2>/dev/null
        return 0
    fi

    # 2. rish (fallback)
    if [ -x "$HOME/.shizuku/rish" ] && "$HOME/.shizuku/rish" -c "echo ready" 2>/dev/null | grep -q ready; then
        "$HOME/.shizuku/rish" -c "$*" 2>/dev/null
        return 0
    fi

    return 1
}
