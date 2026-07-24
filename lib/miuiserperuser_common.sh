# MiuiserPeruser Common Library
#
# Shared shell helpers. Safe to source multiple times.
# Source from anywhere with:
#   source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/<rel-path>/lib/miuiserperuser_common.sh"
# Scripts that source env.sh get this library automatically.

[ -n "${MIUISERPERUSER_COMMON_LOADED:-}" ] && return 0
MIUISERPERUSER_COMMON_LOADED=1

# ------------------------------------------------------------
#  Unified Shell Dispatcher (ShizukuHelper > ADB > rish)
# ------------------------------------------------------------
run_shell() {
    local output

    # 1. ShizukuHelper (most stable for Shizuku)
    if [ -f "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" ]; then
        if output=$(java -jar "$HOME/MiuiserPeruser/tools/ShizukuHelper.jar" "$@" 2>/dev/null) \
           && [ -n "$output" ]; then
            echo "$output"
            return 0
        fi
    fi

    # 2. ADB (reliable when USB connected)
    if command -v adb >/dev/null 2>&1 && adb shell echo ready 2>/dev/null | grep -q ready; then
        adb shell "$@" 2>/dev/null
        return 0
    fi

    # 3. rish (fallback) — known install locations
    local rish
    for rish in "$HOME/.shizuku/rish" "$HOME/rish" "$HOME/Rish/rish"; do
        if [ -x "$rish" ] && RISH_APPLICATION_ID="${RISH_APPLICATION_ID:-com.termux}" \
           "$rish" -c "echo ready" 2>/dev/null | grep -q ready; then
            RISH_APPLICATION_ID="${RISH_APPLICATION_ID:-com.termux}" "$rish" -c "$*" 2>/dev/null
            return 0
        fi
    done

    return 1
}

# ------------------------------------------------------------
#  Log rotation
# ------------------------------------------------------------
MAX_LOG_BYTES="${MAX_LOG_BYTES:-524288}"  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}

# ------------------------------------------------------------
#  Database logging stubs
# ------------------------------------------------------------
#  Overridden by the syndicate DB layer when available; no-ops
#  otherwise so tools run standalone.
# ------------------------------------------------------------
if ! command -v db_log_tool_start >/dev/null 2>&1; then
    db_log_tool_start() { :; }
fi
if ! command -v db_log_tool_end >/dev/null 2>&1; then
    db_log_tool_end() { :; }
fi
if ! command -v db_log_action >/dev/null 2>&1; then
    db_log_action() { :; }
fi
