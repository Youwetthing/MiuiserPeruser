#!/data/data/com.termux/files/usr/bin/bash
# source_tier_lib.sh — Source Tiering Library
# Part of MiuiserPeruser Judicial System v2
#
# Usage (source this file, then call):
#   source source_tier_lib.sh
#   modifier=$(get_tier_modifier "com.example.app")
#   tier=$(get_tier_name "com.example.app")      # for internal logs only
#
# Returns a numeric modifier only. No tier labels written to case files or
# event logs — modifier is silently baked into the weight.
#
# Tier modifier values:
#   MiuiserPeruser daemons     → 0.40
#   Known MIUI system procs    → 0.60
#   Sovereignty-listed apps    → 0.15  (consent gate mandatory regardless of score)
#   Unknown background         → 1.00  (full weight)

# ── Paths ────────────────────────────────────────────────────────────────────

_TIER_BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
_TIER_SOVEREIGNTY_FILE="${_TIER_BASE}/../state/sovereignty.list"

# ── MiuiserPeruser own daemon names (hardcoded) ───────────────────────────────
# Add new daemons here as the fleet grows.

_MIUISER_DAEMON_NAMES=(
    splinterd
    krangd
    rahzerd
    leatherheadd
    metalheadd
    metalhead
    granitord
    ratkingd
    shredderd
    fugitoidd
    bebopd
    burned
    rocksteadyd
    tigerclawd
    turtlecomd
    connectivityd
    networkd
    foot_clan_supreme
    foot_portwatchd
    foot_resurrectord
    foot_ipcshadowd
    footrunner
    cpud
    processd
    storaged
    thermald
    sysportd
    daemonhunterd
    miuiserperuser
    miuiserperuser-daemon
    miuid
    brain-ctl
    april_o_neil
    court_orchestrator
    court_core_engine
    judge_executor
    parole_engine
    scoring_engine
    internal_affairs
    consent_gate
    escalation_daemon
    visitors_pass_daemon
    turtlepower_daemon
    superhero_adapter
    baxter_stockman
)

# ── Known MIUI / HyperOS / AOSP system process prefixes ──────────────────────

_MIUI_PREFIXES=(
    "com.miui."
    "com.xiaomi."
    "com.hyperos."
    "android."
    "com.android."
    "miui."
    "com.lbe."
    "com.qualcomm."
    "com.qti."
    "org.codeaurora."
)

# ── Internal: check if source is a MiuiserPeruser daemon ─────────────────────

_is_miuiser_daemon() {
    local source="$1"
    # Match on binary name (last component if path given)
    local basename
    basename="$(basename "$source")"
    local d
    for d in "${_MIUISER_DAEMON_NAMES[@]}"; do
        [[ "$basename" == "$d" ]] && return 0
    done
    return 1
}

# ── Internal: check if source matches MIUI system prefixes ───────────────────

_is_miui_system() {
    local source="$1"
    local prefix
    for prefix in "${_MIUI_PREFIXES[@]}"; do
        [[ "$source" == ${prefix}* ]] && return 0
    done
    return 1
}

# ── Internal: check sovereignty list ─────────────────────────────────────────
# Sovereignty file format: package.name|PROTECTED|reason|added_epoch
# Returns 0 if listed, 1 if not (or file absent / unreadable)

_is_sovereignty_listed() {
    local source="$1"
    [[ -f "$_TIER_SOVEREIGNTY_FILE" ]] || return 1
    grep -q "^${source}|PROTECTED|" "$_TIER_SOVEREIGNTY_FILE" 2>/dev/null
}

# ── Public: get_tier_modifier <source> ────────────────────────────────────────
# Returns the numeric modifier string (e.g. "0.60").
# Caller uses bc or awk for arithmetic.

get_tier_modifier() {
    local source="$1"
    if [[ -z "$source" ]]; then
        echo "1.00"
        return
    fi

    if _is_miuiser_daemon "$source"; then
        echo "0.40"
    elif _is_sovereignty_listed "$source"; then
        echo "0.15"
    elif _is_miui_system "$source"; then
        echo "0.60"
    else
        echo "1.00"
    fi
}

# ── Public: get_tier_name <source> ────────────────────────────────────────────
# Returns a short internal label used only in scoring_reasoning.log.
# NEVER written to case files, court.events, or user-facing logs.

get_tier_name() {
    local source="$1"
    if [[ -z "$source" ]]; then
        echo "unknown"
        return
    fi

    if _is_miuiser_daemon "$source"; then
        echo "miuiser_daemon"
    elif _is_sovereignty_listed "$source"; then
        echo "sovereignty"
    elif _is_miui_system "$source"; then
        echo "miui_system"
    else
        echo "unknown_background"
    fi
}

# ── Public: is_sovereignty_app <source> ───────────────────────────────────────
# Convenience wrapper — returns 0/1. Used by consent_gate.sh to force gate.

is_sovereignty_app() {
    _is_sovereignty_listed "$1"
}

# ── Public: is_miuiser_daemon <source> ────────────────────────────────────────
# Used by internal_affairs.sh and scoring_engine to route to IA before scoring.

is_miuiser_daemon() {
    _is_miuiser_daemon "$1"
}

# ── Selftest (run directly: bash source_tier_lib.sh --test) ──────────────────

if [[ "${BASH_SOURCE[0]}" == "$0" && "$1" == "--test" ]]; then
    echo "=== source_tier_lib selftest ==="
    tests=(
        "splinterd:0.40:miuiser_daemon"
        "rahzerd:0.40:miuiser_daemon"
        "com.miui.home:0.60:miui_system"
        "com.android.settings:0.60:miui_system"
        "com.xiaomi.bluetooth:0.60:miui_system"
        "com.example.unknown:1.00:unknown_background"
        "com.evil.tracker:1.00:unknown_background"
    )
    pass=0; fail=0
    for t in "${tests[@]}"; do
        IFS=: read -r src expected_mod expected_tier <<< "$t"
        mod=$(get_tier_modifier "$src")
        tier=$(get_tier_name "$src")
        if [[ "$mod" == "$expected_mod" && "$tier" == "$expected_tier" ]]; then
            echo "  PASS  $src → mod=$mod tier=$tier"
            ((pass++))
        else
            echo "  FAIL  $src → got mod=$mod tier=$tier (expected mod=$expected_mod tier=$expected_tier)"
            ((fail++))
        fi
    done
    echo "--- $pass passed, $fail failed ---"
fi
