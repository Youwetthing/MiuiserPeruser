#!/data/data/com.termux/files/usr/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# tee_lockdown.sh — Manual TEE/biometric daemon freeze-thaw
#
# Confirmed live 2026-07-14: SELinux fully blocks shell access to every
# actual TEE device node (/dev/isee_tee0, /dev/teei_fp, /dev/ut_keymaster,
# etc.) — no exploit, no privilege escalation, no real security boundary
# crossed here. All this does is SIGSTOP/SIGCONT ordinary userspace
# daemons (teei_daemon, SOTER, THH, fingerprint HAL), same as freezing any
# other background app. Fully reversible, standard process control.
#
# Deliberately kept OUT of granitord and the automatic scan fleet — this
# is a manual, explicit, one-off action, not something any scan loop
# should ever trigger on its own. Freezing SOTER/hardware-keystore can
# still affect apps that use hardware-backed key storage or attestation
# even without biometrics enabled (some banking/password-manager flows).
#
# PIDs are looked up live by process name every time this runs — NEVER
# hardcode PIDs, they aren't stable across reboots or service restarts.
#
# Usage: tee_lockdown.sh {freeze|thaw|status}
# ═══════════════════════════════════════════════════════════════════════════

RISH_APPLICATION_ID=com.termux

# name -> friendly label, used for both lookup and display
declare -A TEE_TARGETS=(
    [teei_daemon]="TEE daemon (kernel<->secure-world bridge)"
    [vendor.microtrust.hardware.soter-service]="SOTER (FIDO/WebAuthn secure auth)"
    [vendor.microtrust.hardware.thh-service]="THH (TEE helper HAL)"
    [android.hardware.biometrics.fingerprint-service.odm]="Fingerprint HAL"
)

RISH="$HOME/Rish/rish"

_rish() {
    RISH_APPLICATION_ID=com.termux "$RISH" -c "$1" 2>/dev/null
}

# Looks up the live PID for a process by exact name match on the first
# ps column — not a substring grep against the full command line, since
# teei_daemon's real argv contains long hex blobs that could coincidentally
# match other patterns.
_find_pid() {
    local name="$1"
    _rish "ps -A -o PID,ARGS | awk -v n=\"$name\" '\$2==n{print \$1; exit}'"
}

cmd_status() {
    printf "\n  TEE/biometric daemon status:\n\n"
    for name in "${!TEE_TARGETS[@]}"; do
        local pid
        pid=$(_find_pid "$name")
        if [ -z "$pid" ]; then
            printf "  ○ %-55s not running\n" "${TEE_TARGETS[$name]}"
            continue
        fi
        local state
        state=$(_rish "cat /proc/$pid/status 2>/dev/null | grep '^State:'" | awk '{print $2, $3}')
        printf "  ● %-55s pid=%-6s state=%s\n" "${TEE_TARGETS[$name]}" "$pid" "$state"
    done
    printf "\n"
}

cmd_freeze() {
    printf "\n  ⚠  This will SIGSTOP the following live services:\n\n"
    local -A found_pids
    for name in "${!TEE_TARGETS[@]}"; do
        local pid
        pid=$(_find_pid "$name")
        if [ -n "$pid" ]; then
            found_pids[$name]="$pid"
            printf "    - %-55s pid=%s\n" "${TEE_TARGETS[$name]}" "$pid"
        fi
    done

    if [ ${#found_pids[@]} -eq 0 ]; then
        printf "\n  Nothing found running. Nothing to freeze.\n\n"
        return 0
    fi

    printf "\n  This will disrupt any in-progress hardware-backed key\n"
    printf "  operations, FIDO/WebAuthn auth, or fingerprint HAL calls\n"
    printf "  for as long as it's frozen. Fully reversible via 'thaw'.\n\n"
    printf "  Type FREEZE to confirm: "
    read -r confirm
    if [ "$confirm" != "FREEZE" ]; then
        printf "\n  Not confirmed. Aborting.\n\n"
        return 1
    fi

    for name in "${!found_pids[@]}"; do
        local pid="${found_pids[$name]}"
        _rish "kill -STOP $pid" >/dev/null
        printf "  ✔ Frozen: %-55s pid=%s\n" "${TEE_TARGETS[$name]}" "$pid"
    done
    printf "\n  Use 'tee_lockdown.sh thaw' to resume.\n\n"
}

cmd_thaw() {
    printf "\n  Resuming TEE/biometric services...\n\n"
    local any=0
    for name in "${!TEE_TARGETS[@]}"; do
        local pid
        pid=$(_find_pid "$name")
        if [ -n "$pid" ]; then
            _rish "kill -CONT $pid" >/dev/null
            printf "  ✔ Resumed: %-55s pid=%s\n" "${TEE_TARGETS[$name]}" "$pid"
            any=1
        fi
    done
    [ $any -eq 0 ] && printf "  Nothing found running.\n"
    printf "\n"
}

case "$1" in
    freeze) cmd_freeze ;;
    thaw)   cmd_thaw ;;
    status) cmd_status ;;
    *)
        printf "usage: %s {freeze|thaw|status}\n" "$0"
        exit 2
        ;;
esac
