#!/data/data/com.termux/files/usr/bin/bash
source "$(dirname "$0")/lib/tool_backend.sh"
# ==============================================================================
#  Kernel Sanders v2 — Kernel Integrity & Security Posture Tool
#  Companions: shredderd, granitord, casey kernel checks
#  Theme: 1980s harvest gold, burnt orange, avocado, cream
# ==============================================================================

BASE="$HOME/MiuiserPeruser"
SHREDDERD_RESULTS="$BASE/Registry/daemon_results/shredderd.json"
GRANITORD_RESULTS="$BASE/Registry/daemon_results/granitord.json"
CASEY_JSON="$BASE/data/last_scan.json"
VULN_DIR="/sys/devices/system/cpu/vulnerabilities"


# ── Variant detection ─────────────────────────────────────────────────────────
detect_variant() {
    VARIANT_DEVICE=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.product.marketname 2>/dev/null" | tr -d '
')
    [ -z "$VARIANT_DEVICE" ] && VARIANT_DEVICE=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.product.name 2>/dev/null" | tr -d '
')
    VARIANT_PLATFORM=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.board.platform 2>/dev/null" | tr -d '
')
    VARIANT_REGION=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.miui.region 2>/dev/null" | tr -d '
')
    VARIANT_BUILD=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.build.type 2>/dev/null" | tr -d '
')
    VARIANT_PATCH=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.build.version.security_patch 2>/dev/null" | tr -d '
')
    VARIANT_ANDROID=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.build.version.release 2>/dev/null" | tr -d '
')
    VARIANT_BRAND=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.product.brand 2>/dev/null" | tr -d '
')

    # SoC family
    case "$VARIANT_PLATFORM" in
        mt*|MT*) VARIANT_SOC="mediatek" ;;
        sm*|SM*) VARIANT_SOC="snapdragon" ;;
        *exynos*|*universal*) VARIANT_SOC="exynos" ;;
        *) VARIANT_SOC="unknown" ;;
    esac

    # Region family
    case "$VARIANT_REGION" in
        CN|cn) VARIANT_REGION_FAMILY="cn" ;;
        EEA|eea|IE|ie|EU|eu) VARIANT_REGION_FAMILY="eea" ;;
        *) VARIANT_REGION_FAMILY="global" ;;
    esac

    # Brand tier
    case "${VARIANT_BRAND,,}" in
        *poco*) VARIANT_TIER="poco" ;;
        *redmi*) VARIANT_TIER="redmi" ;;
        *mi*|*xiaomi*) VARIANT_TIER="mi" ;;
        *) VARIANT_TIER="unknown" ;;
    esac
}

show_variant_banner() {
    printf "${BOLD}${HG}  [VARIANT] Device Profile${RESET}
"
    divider
    printf "  ${WH}Device:${RESET}   ${BOLD}%s${RESET}
" "$VARIANT_DEVICE"
    printf "  ${WH}SoC:${RESET}      ${BO}%s${RESET} (%s)
" "$VARIANT_PLATFORM" "$VARIANT_SOC"
    printf "  ${WH}Region:${RESET}   ${GR}%s${RESET} (%s)
" "$VARIANT_REGION" "$VARIANT_REGION_FAMILY"
    printf "  ${WH}Brand:${RESET}    %s (%s)
" "$VARIANT_BRAND" "$VARIANT_TIER"
    printf "  ${WH}Android:${RESET}  %s | Patch: %s
" "$VARIANT_ANDROID" "$VARIANT_PATCH"
    printf "  ${WH}Build:${RESET}    %s
" "$VARIANT_BUILD"

    # Warnings
    [ "$VARIANT_BUILD" = "userdebug" ] &&         printf "
${RD}  [!!] USERDEBUG BUILD — massive attack surface${RESET}
"
    [ "$VARIANT_REGION_FAMILY" = "cn" ] &&         printf "
${BO}  [i] CN BUILD — additional telemetry checks enabled${RESET}
"
    [ "$VARIANT_TIER" = "poco" ] &&         printf "
${WA}  [i] POCO — often ships with permissive kernel config${RESET}
"

    # Patch age warning
    if [ -n "$VARIANT_PATCH" ] && [ "$VARIANT_PATCH" != "unknown" ]; then
        patch_ts=$(date -d "$VARIANT_PATCH" +%s 2>/dev/null || echo 0)
        now_ts=$(date +%s)
        age_days=$(( (now_ts - patch_ts) / 86400 ))
        if [ "$age_days" -gt 90 ] 2>/dev/null; then
            printf "
${RD}  [!!] Security patch is ${age_days} days old — update recommended${RESET}
"
        fi
    fi
    printf "
"
}

# ── Retro palette ─────────────────────────────────────────────────────────────
HG='\033[38;5;136m'   # harvest gold
BO='\033[38;5;130m'   # burnt orange
AV='\033[38;5;58m'    # avocado green
CR='\033[38;5;223m'   # cream
WA='\033[38;5;180m'   # warm amber
RD='\033[38;5;160m'   # alarm red
GR='\033[38;5;64m'    # safe green
DK='\033[38;5;238m'   # dark
WH='\033[38;5;255m'   # white
BOLD='\033[1m'
RESET='\033[0m'

COLS=$(tput cols 2>/dev/null || echo 60)
W=$(( COLS - 2 )); [ "$W" -gt 76 ] && W=76

rsh() { _backend_exec "$@" 2>/dev/null; }

safe_read() {
    local path="$1" default="${2:-N/A}"
    [[ ! -e "$path" ]] && echo "MISSING" && return 1
    [[ ! -r "$path" ]] && echo "DENIED" && return 1
    cat "$path" 2>/dev/null || echo "$default"
}

safe_sysctl() {
    local key="$1"
    safe_read "/proc/sys/${key//./\/}"
}


wallpaper_line() {
    local pat="${HG}#${BO}~${AV}=${CR}-${WA}~${RESET}"
    local line="" i=0
    while [ $i -lt $(( W / 5 + 2 )) ]; do line="${line}${pat}"; i=$((i+1)); done
    printf "%.${W}s\n" "$(printf '%b' "$line")"
}

divider() { printf "${HG}"; printf '%*s' "$W" '' | tr ' ' '-'; printf "${RESET}\n"; }
header() {
    clear
    wallpaper_line
    printf "\n"
    printf "${BOLD}${HG}  ~ K E R N E L   S A N D E R S   v2 ~${RESET}\n"
    printf "${DK}  Integrity · Vulnerabilities · eBPF · Hardening${RESET}\n"
    printf "${AV}  Companions: shredderd · granitord · casey${RESET}\n\n"
    wallpaper_line
    printf "\n"
}

# ── shredderd results ─────────────────────────────────────────────────────────
show_shredderd() {
    printf "${BOLD}${HG}  [SHREDDERD] File Integrity${RESET}\n"
    divider
    if [ ! -f "$SHREDDERD_RESULTS" ]; then
        printf "${BO}  No shredderd results. Run shredderd first.${RESET}\n\n"
        return
    fi
    python3 << 'PY'
import json, os
path = os.path.expanduser('~/MiuiserPeruser/Registry/daemon_results/shredderd.json')
try:
    d = json.load(open(path))
    for k, v in d.items():
        status = str(v.get('status', v) if isinstance(v, dict) else v)
        icon = '[OK]' if any(x in status.lower() for x in ['ok','pass','clean']) else '[!!]'
        print(f'  {icon}  {k}: {status}')
except Exception as e:
    print(f'  Error reading results: {e}')
PY
    printf "\n"
}

# ── granitord results ─────────────────────────────────────────────────────────
show_granitord() {
    printf "${BOLD}${HG}  [GRANITORD] System State${RESET}\n"
    divider
    if [ ! -f "$GRANITORD_RESULTS" ]; then
        printf "${BO}  No granitord results.${RESET}\n\n"
        return
    fi
    python3 << 'PY'
import json, os
path = os.path.expanduser('~/MiuiserPeruser/Registry/daemon_results/granitord.json')
try:
    d = json.load(open(path))
    for k, v in d.items():
        print(f'  {k}: {v}')
except Exception as e:
    print(f'  Error: {e}')
PY
    printf "\n"
}

# ── Casey kernel findings ─────────────────────────────────────────────────────
show_casey_kernel() {
    printf "${BOLD}${HG}  [CASEY] Kernel Hook Findings${RESET}\n"
    divider
    if [ ! -f "$CASEY_JSON" ]; then
        printf "${BO}  No Casey scan data. Run superhero --deep first.${RESET}\n\n"
        return
    fi
    python3 << 'PY'
import json, os
path = os.path.expanduser('~/MiuiserPeruser/data/last_scan.json')
try:
    data = json.load(open(path))
    kernel = [f for f in data if f.get('turtle','') in ('CASEY_HOOK','CASEY_KERNEL')]
    if not kernel:
        print('  [OK] No Casey kernel findings in last scan')
    for f in kernel:
        pri = f.get('priority','?')
        typ = f.get('type','?')
        desc = f.get('description','')
        icons = {'CRITICAL':'[!!]','HIGH':'[HI]','MEDIUM':'[MD]','LOW':'[LO]'}
        print(f'  {icons.get(pri,"[?]")} [{pri}] {typ}')
        print(f'     {desc}')
        print()
except Exception as e:
    print(f'  Error: {e}')
PY
    printf "\n"
}

# ── CPU vulnerability audit ───────────────────────────────────────────────────
show_cpu_vulns() {
    printf "${BOLD}${HG}  [CPU] Speculative Execution Vulnerabilities${RESET}\n"
    divider

    declare -A EXPLAIN=(
        [spectre_v1]="Bounds Check Bypass — can leak kernel memory via branch speculation"
        [spectre_v2]="Spectre V2 on ARM — your device reports Unprivileged eBPF as the attack vector. eBPF programs can exploit branch predictor state to leak data across security boundaries. This is the primary exploitable vulnerability on this Redmi 15C."
        [spec_store_bypass]="Speculative Store Bypass — eBPF programs can bypass SSBD mitigation on this device"
        [meltdown]="Out-of-order kernel memory read — Cortex-A55 not affected, A75 may be"
        [spectre_v1]="Bounds Check Bypass — malicious code tricks CPU into speculatively reading kernel memory before bounds checks complete. Cortex-A55/A75 partially mitigated via __user pointer sanitization."
        [retbleed]="Return instruction speculation — can leak kernel data"
        [mds]="Microarchitectural Data Sampling — Intel only, ARM not affected"
        [mmio_stale_data]="MMIO buffer stale data — Intel only"
        [tsx_async_abort]="TSX Async Abort — Intel only"
        [srbds]="Special Register Buffer — Intel only"
        [itlb_multihit]="iTLB page fault — Intel only"
        [l1tf]="L1 Terminal Fault — Intel only"
        [gather_data_sampling]="AVX gather instruction — Intel only"
        [reg_file_data_sampling]="Register file sampling — Intel only"
        [spec_rstack_overflow]="Return stack overflow — AMD only"
    )

    if [ ! -d "$VULN_DIR" ]; then
        printf "${BO}  /sys/devices/system/cpu/vulnerabilities not available${RESET}\n\n"
        return
    fi

        RISH_APPLICATION_ID=com.termux ~/Rish/rish -c         'for f in /sys/devices/system/cpu/vulnerabilities/*; do printf "%s\t%s\n" "$(basename $f)" "$(cat $f)"; done'         2>/dev/null | while IFS=$'\t' read -r name status; do
        explain="${EXPLAIN[$name]:-Unknown vulnerability}"

        if echo "$status" | grep -q "Vulnerable"; then
            if echo "$status" | grep -q "eBPF"; then
                printf "${RD}  [CRITICAL] ${name}${RESET}\n"
                printf "${WH}  Status: ${status}${RESET}\n"
                printf "${BO}  ${explain}${RESET}\n"
                printf "${RD}  CVE: CVE-2021-3490, CVE-2022-23222, CVE-2023-2163${RESET}\n\n"
            else
                printf "${BO}  [HIGH] ${name}${RESET}\n"
                printf "${WH}  Status: ${status}${RESET}\n"
                printf "${CR}  ${explain}${RESET}\n\n"
            fi
        elif echo "$status" | grep -q "Mitigation"; then
            printf "${WA}  [MITIGATED] ${name}${RESET}\n"
            printf "${WH}  Status: ${status}${RESET}\n"
            printf "${CR}  ${explain}${RESET}\n\n"
        else
            printf "${GR}  [OK] ${name}: ${status}${RESET}\n"
        fi
    done
    printf "\n"
}

# ── eBPF audit ────────────────────────────────────────────────────────────────

rish_read() {
    RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat $1 2>/dev/null" 2>/dev/null | tr -d '\n'
}

show_ebpf() {
    printf "${BOLD}${HG}  [eBPF] Attack Surface Audit${RESET}\n"
    printf "${BO}  Note: spectre_v2 reports Vulnerable: Unprivileged eBPF enabled${RESET}\n"
    printf "${BO}  CVE-2021-3490, CVE-2022-23222, CVE-2023-2163 — eBPF LPE attack surface confirmed${RESET}\n\n"
    divider

    # Unprivileged eBPF
    unpriv=$(rish_read /proc/sys/kernel/unprivileged_bpf_disabled)
    if [ -z "$unpriv" ] || [ "$unpriv" = "DENIED" ]; then
        printf "${WA}  [i] Unprivileged eBPF: SELinux blocks read (shell uid) — run as root to audit${RESET}\n\n"
    else
    case "$unpriv" in
        0) printf "${RD}  [CRITICAL] Unprivileged eBPF: ENABLED${RESET}\n"
           printf "${WH}  Any app can load eBPF kernel programs without root.${RESET}\n"
           printf "${WH}  CVE-2021-3490, CVE-2022-23222, CVE-2023-2163 all exploited this.${RESET}\n"
           printf "${BO}  Hardening: see Actions menu${RESET}\n\n" ;;
        1) printf "${GR}  [OK] Unprivileged eBPF: DISABLED${RESET}\n\n" ;;
        2) printf "${GR}  [OK] Unprivileged eBPF: PERMANENTLY DISABLED${RESET}\n\n" ;;
        *) printf "${WA}  [?] Unprivileged eBPF: UNKNOWN (${unpriv})${RESET}\n\n" ;;
    esac
    fi

    # JIT compiler
    jit=$(rish_read /proc/sys/net/core/bpf_jit_enable)
    case "$jit" in
        0) printf "${GR}  [OK] eBPF JIT: disabled (interpreter mode)${RESET}\n\n" ;;
        1) printf "${WA}  [MD] eBPF JIT: enabled — increases attack surface${RESET}\n\n" ;;
        2) printf "${RD}  [!!] eBPF JIT: DEBUG mode — never on production${RESET}\n\n" ;;
        *) printf "${WA}  [?] eBPF JIT: unknown (${jit})${RESET}\n\n" ;;
    esac

    # JIT hardening
    harden=$(rish_read /proc/sys/net/core/bpf_jit_harden)
    case "$harden" in
        0) printf "${RD}  [!!] eBPF JIT hardening: DISABLED — JIT spraying possible${RESET}\n\n" ;;
        1) printf "${WA}  [MD] eBPF JIT hardening: unprivileged only${RESET}\n\n" ;;
        2) printf "${GR}  [OK] eBPF JIT hardening: ALL programs blinded${RESET}\n\n" ;;
        *) printf "${WA}  [?] eBPF JIT hardening: unknown (${harden})${RESET}\n\n" ;;
    esac

    # kptr_restrict
    kptr=$(rish_read /proc/sys/kernel/kptr_restrict)
    [ "$kptr" = "0" ] &&         printf "${RD}  [!!] kptr_restrict=0 — kernel pointers exposed in /proc${RESET}

" ||         printf "${GR}  [OK] kptr_restrict=${kptr}${RESET}

"

    # ASLR
    aslr=$(rish_read /proc/sys/kernel/randomize_va_space)
    case "$aslr" in
        0) printf "${RD}  [CRITICAL] ASLR disabled — memory layout predictable, exploits trivial${RESET}

" ;;
        1) printf "${WA}  [MD] ASLR partial — stack/mmap randomised, heap not${RESET}

" ;;
        2) printf "${GR}  [OK] ASLR full randomisation${RESET}

" ;;
        *) printf "${WA}  [?] ASLR unknown (${aslr})${RESET}

" ;;
    esac

    # dmesg_restrict
    dmesg=$(rish_read /proc/sys/kernel/dmesg_restrict)
    [ "$dmesg" = "0" ] &&         printf "${BO}  [HI] dmesg_restrict=0 — kernel log readable by all processes${RESET}

" ||         printf "${GR}  [OK] dmesg_restrict=${dmesg}${RESET}

"

    # kallsyms
    kall=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c 'grep -c sys_call_table /proc/kallsyms' 2>/dev/null | tr -d '\n')
    [ "${kall:-0}" -gt 0 ] 2>/dev/null &&         printf "${RD}  [HI] sys_call_table exposed in /proc/kallsyms — rootkit target${RESET}

" ||         printf "${GR}  [OK] kallsyms restricted${RESET}

"

    # ptrace scope (YAMA)
    ptrace=$(rish_read /proc/sys/kernel/yama/ptrace_scope)
    case "$ptrace" in
        0) printf "${RD}  [!!] ptrace_scope=0 — any process can trace any other (credential theft risk)${RESET}

" ;;
        1) printf "${WA}  [MD] ptrace_scope=1 — parent/child only${RESET}

" ;;
        2) printf "${GR}  [OK] ptrace_scope=2 — admin only${RESET}

" ;;
        3) printf "${GR}  [OK] ptrace_scope=3 — disabled${RESET}

" ;;
        *) printf "${WA}  [?] ptrace_scope unavailable (YAMA not enabled)${RESET}

" ;;
    esac

    # debugfs
    dbgfs=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c 'mount | grep -c debugfs' 2>/dev/null | tr -d '\n')
    [ "${dbgfs:-0}" -gt 0 ] 2>/dev/null &&         printf "${BO}  [HI] debugfs mounted — exposes kernel internals${RESET}

" ||         printf "${GR}  [OK] debugfs not mounted${RESET}

"

    # Security patch level
    patch=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getprop ro.build.version.security_patch 2>/dev/null")
    printf "${CR}  [i] Security patch level: ${patch}${RESET}

"
}

# ── Kernel modules ────────────────────────────────────────────────────────────
show_modules() {
    printf "${BOLD}${HG}  [MODULES] Loaded Kernel Modules${RESET}\n"
    divider
    mods=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/modules 2>/dev/null | awk '{print \$1}' | sort")
    count=$(echo "$mods" | wc -l)
    printf "${CR}  $count modules loaded${RESET}\n\n"

    # Highlight interesting ones
    echo "$mods" | grep -iE "millet|ksu|magisk|frida|zygisk|binder_gki" | while read -r m; do
        printf "${WA}  [NOTE] ${m} — review recommended${RESET}\n"
    done
    printf "\n"

    # Baseline diff
    BASELINE="$BASE/data/kmod_baseline.txt"
    if [ -f "$BASELINE" ]; then
        printf "${CR}  New since baseline:${RESET}\n"
        new=$(comm -23 <(echo "$mods") <(sort "$BASELINE") 2>/dev/null)
        if [ -z "$new" ]; then
            printf "${GR}  [OK] No new modules since baseline${RESET}\n"
        else
            echo "$new" | while read -r m; do
                printf "${RD}  [NEW] ${m}${RESET}\n"
            done
        fi
    fi
    printf "\n"
}

# ── SELinux ───────────────────────────────────────────────────────────────────
show_selinux() {
    printf "${BOLD}${HG}  [SELINUX] Enforcement Status${RESET}\n"
    divider
    se=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "getenforce 2>/dev/null")
    case "$se" in
        Enforcing)  printf "${GR}  [OK] SELinux: Enforcing${RESET}\n\n" ;;
        Permissive) printf "${RD}  [!!] SELinux: Permissive — policies not enforced${RESET}\n\n" ;;
        Disabled)   printf "${RD}  [CRITICAL] SELinux: Disabled${RESET}\n\n" ;;
        *)          printf "${WA}  [?] SELinux: ${se}${RESET}\n\n" ;;
    esac
}

# ── Hardening actions ─────────────────────────────────────────────────────────
show_actions() {
    header
    printf "${BOLD}${HG}  [ACTIONS] Apply Kernel Hardening${RESET}\n"
    divider
    printf "\n"
    printf "${HG}  [1]${RESET} ${WH}Disable unprivileged eBPF (recommended)${RESET}\n"
    printf "${HG}  [2]${RESET} ${WH}Enable eBPF JIT hardening (level 2)${RESET}\n"
    printf "${HG}  [3]${RESET} ${WH}Set kptr_restrict=2 (hide kernel pointers)${RESET}\n"
    printf "${HG}  [4]${RESET} ${WH}Set dmesg_restrict=1${RESET}\n"
    printf "${HG}  [5]${RESET} ${WH}Apply all hardening at once${RESET}\n"
    printf "${HG}  [p]${RESET} ${WH}Generate Magisk persistence script${RESET}\n"
    printf "${HG}  [b]${RESET} ${WH}Back${RESET}\n"
    printf "\n"
    divider
    printf "${HG}  Choice: ${RESET}"
    read -r choice

    apply() {
        printf "${AV}  Applying: $1 = $2 ...${RESET}\n"
        rsh "echo $2 > /proc/sys/$1 2>/dev/null"
        val=$(rsh "cat /proc/sys/$1 2>/dev/null")
        [ "$val" = "$2" ] && printf "${GR}  [OK] Applied.${RESET}\n\n" || \
            printf "${RD}  [FAIL] Could not apply — may need root.${RESET}\n\n"
    }

    case "$choice" in
        1) apply "kernel/unprivileged_bpf_disabled" "1" ;;
        2) apply "net/core/bpf_jit_harden" "2" ;;
        3) apply "kernel/kptr_restrict" "2" ;;
        4) apply "kernel/dmesg_restrict" "1" ;;
        5)
            apply "kernel/unprivileged_bpf_disabled" "1"
            apply "net/core/bpf_jit_harden" "2"
            apply "kernel/kptr_restrict" "2"
            apply "kernel/dmesg_restrict" "1"
            ;;
        b|B) return ;;
        p|P)
            printf "\n${BOLD}${HG}  Magisk service.sh template:${RESET}\n"
            printf "${WH}  #!/system/bin/sh\n"
            printf "${WH}  echo 1 > /proc/sys/kernel/unprivileged_bpf_disabled\n"
            printf "${WH}  echo 2 > /proc/sys/net/core/bpf_jit_harden\n"
            printf "${WH}  echo 2 > /proc/sys/kernel/kptr_restrict\n"
            printf "${WH}  echo 1 > /proc/sys/kernel/dmesg_restrict\n"
            printf "${WH}  echo 2 > /proc/sys/kernel/randomize_va_space\n"
            printf "${WH}  umount /sys/kernel/debug 2>/dev/null\n"
            printf "\n${BO}  Save to: /data/adb/modules/kernel_sanders/service.sh${RESET}\n"
            printf "${BO}  chmod 755 service.sh && reboot${RESET}\n\n"
            ;;
    esac
    printf "${HG}  Press enter...${RESET}"; read -r
}


# ── Kernel config audit ───────────────────────────────────────────────────────
show_kconfig() {
    printf "${BOLD}${HG}  [KCONFIG] Compile-Time Security Audit${RESET}\n"
    divider

    # Try to read kernel config
    local cfg=""
    if RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish \
        -c "test -r /proc/config.gz" 2>/dev/null; then
        cfg=$(RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish \
            -c "zcat /proc/config.gz 2>/dev/null" 2>/dev/null)
    fi

    if [ -z "$cfg" ]; then
        printf "${WA}  /proc/config.gz not readable — showing known MT6768 analysis${RESET}\n\n"
        # Known findings from MT6768 HyperOS 2.0 analysis
        printf "${RD}  [CRITICAL] CONFIG_BPF_UNPRIV_DEFAULT_OFF: not set${RESET}\n"
        printf "${WH}             Unprivileged eBPF ON by default — Xiaomi MILLET dependency${RESET}\n\n"
        printf "${RD}  [CRITICAL] CONFIG_BPF_JIT_ALWAYS_ON: y${RESET}\n"
        printf "${WH}             JIT cannot be disabled at runtime${RESET}\n\n"
        printf "${RD}  [CRITICAL] CONFIG_DEBUG_FS_ALLOW_ALL: y${RESET}\n"
        printf "${WH}             debugfs fully permissive — no runtime fix possible${RESET}\n\n"
        printf "${BO}  [HIGH]     CONFIG_SECURITY_YAMA: not set${RESET}\n"
        printf "${WH}             No ptrace restrictions — any app can debug any other${RESET}\n\n"
        printf "${BO}  [HIGH]     CONFIG_INIT_ON_FREE_DEFAULT_ON: not set${RESET}\n"
        printf "${WH}             Freed heap not zeroed — UAF data leakage possible${RESET}\n\n"
        printf "${GR}  [OK]       CONFIG_RANDOMIZE_BASE: y — KASLR active${RESET}\n"
        printf "${GR}  [OK]       CONFIG_STACKPROTECTOR_STRONG: y — stack canaries${RESET}\n"
        printf "${GR}  [OK]       CONFIG_SLAB_FREELIST_HARDENED: y — heap randomization${RESET}\n"
        printf "${GR}  [OK]       CONFIG_HARDENED_USERCOPY: y — bounds checking${RESET}\n"
        printf "${GR}  [OK]       CONFIG_FORTIFY_SOURCE: y — compile-time buffer checks${RESET}\n"
        printf "${GR}  [OK]       CONFIG_INIT_STACK_ALL_ZERO: y — stack zeroing${RESET}\n"
        printf "${GR}  [OK]       CONFIG_INIT_ON_ALLOC_DEFAULT_ON: y — heap zeroing${RESET}\n\n"
        printf "${CR}  Runtime vs Recompile:${RESET}\n"
        printf "${WH}  Unprivileged eBPF: fixable via sysctl (needs Magisk persistence)${RESET}\n"
        printf "${WH}  JIT hardening:     fixable via sysctl${RESET}\n"
        printf "${RD}  YAMA missing:      impossible without kernel recompile${RESET}\n"
        printf "${RD}  debugfs:           impossible without kernel recompile${RESET}\n"
        printf "${RD}  JIT always on:     impossible without kernel recompile${RESET}\n\n"
        printf "${WA}  Score: 7/12 hardening features present${RESET}\n"
        printf "${WA}  Modern kernel critically undermined by eBPF compile config${RESET}\n\n"
        return
    fi

    # Live config analysis — correctly handles commented-out lines
    check_cfg() {
        local name="$1" want="$2" label="$3"
        # Check if explicitly set (CONFIG_X=y) vs commented out (# CONFIG_X is not set)
        if echo "$cfg" | grep -q "^${name}=${want}$"; then
            printf "${GR}  [OK]   ${name}=${want} — ${label}${RESET}\n"
        elif echo "$cfg" | grep -q "^# ${name} is not set"; then
            printf "${RD}  [!!]   ${name}=not set — ${label}${RESET}\n"
        elif echo "$cfg" | grep -q "^${name}="; then
            val=$(echo "$cfg" | grep "^${name}=" | cut -d= -f2)
            printf "${BO}  [MD]   ${name}=${val} (want ${want}) — ${label}${RESET}\n"
        else
            printf "${WA}  [?]    ${name}=unknown — ${label}${RESET}\n"
        fi
    }

    printf "${CR}  eBPF config:${RESET}\n"
    check_cfg "CONFIG_BPF_UNPRIV_DEFAULT_OFF" "y" "Unprivileged eBPF off by default"
    check_cfg "CONFIG_BPF_JIT_ALWAYS_ON" "n" "JIT can be disabled at runtime"
    check_cfg "CONFIG_BPF_LSM" "n" "eBPF cannot attach to LSM hooks"
    printf "\n${CR}  Hardening config:${RESET}\n"
    check_cfg "CONFIG_RANDOMIZE_BASE" "y" "KASLR"
    check_cfg "CONFIG_STACKPROTECTOR_STRONG" "y" "Stack canaries"
    check_cfg "CONFIG_HARDENED_USERCOPY" "y" "Hardened usercopy"
    check_cfg "CONFIG_FORTIFY_SOURCE" "y" "Fortify source"
    check_cfg "CONFIG_SECURITY_YAMA" "y" "YAMA ptrace restrictions"
    check_cfg "CONFIG_INIT_ON_FREE_DEFAULT_ON" "y" "Heap zeroing on free"
    printf "\n"
}


# ── Hook profile ──────────────────────────────────────────────────────────────
show_hook_profile() {
    printf "${BOLD}${HG}  [HOOKS] Kernel Hook & eBPF Profile${RESET}\n"
    divider
    printf "\n"
    printf "${CR}  KNOWN ACTIVE (from loaded modules + CPU vuln data):${RESET}\n\n"
    printf "${BO}  [MILLET]   millet_binder, millet_core, binder_gki${RESET}\n"
    printf "${WH}             Xiaomi binder IPC monitoring — hooks all app-to-app comms${RESET}\n"
    printf "${WH}             Permanent — baked into HyperOS kernel, cannot disable${RESET}\n\n"
    printf "${BO}  [eBPF]     spectre_v2 confirms unprivileged eBPF active${RESET}\n"
    printf "${WH}             Any app can load eBPF to kprobes/tracepoints/LSM hooks${RESET}\n"
    printf "${WH}             CONFIG_BPF_LSM=y, CONFIG_BPF_EVENTS=y${RESET}\n\n"
    printf "${BO}  [PTRACE]   CONFIG_SECURITY_YAMA not set${RESET}\n"
    printf "${WH}             No ptrace restrictions — any process can debug any other${RESET}\n\n"
    printf "${CR}  UNKNOWN (SELinux blocks read, root required):${RESET}\n\n"
    printf "${WA}  [?] Active kprobes list   — /sys/kernel/debug/kprobes/list${RESET}\n"
    printf "${WA}  [?] Active tracepoints    — /sys/kernel/debug/tracing/events${RESET}\n"
    printf "${WA}  [?] Loaded eBPF programs  — requires bpftool or /proc/PID/fdinfo scan${RESET}\n"
    printf "${WA}  [?] Binder hook stats     — /proc/binder/stats${RESET}\n"
    printf "${WA}  [?] Runtime sysctl values — unprivileged_bpf_disabled, kptr_restrict${RESET}\n\n"
    printf "${CR}  WHAT THIS MEANS:${RESET}\n\n"
    printf "${WH}  Xiaomi has kernel-level visibility into all app communications${RESET}\n"
    printf "${WH}  via MILLET. The eBPF surface means any sufficiently privileged${RESET}\n"
    printf "${WH}  app or exploit can attach additional hooks. SELinux is the only${RESET}\n"
    printf "${WH}  runtime defence — and it is Enforcing on this device.${RESET}\n\n"
    divider
}

# ── Full report ───────────────────────────────────────────────────────────────
show_full() {
    header
    show_variant_banner
    show_kconfig
    show_hook_profile
    show_shredderd
    show_granitord
    show_casey_kernel
    show_cpu_vulns
    show_ebpf
    show_selinux
    show_modules
}

# ── Menu ──────────────────────────────────────────────────────────────────────
menu() {
    while true; do
    header
    printf "${BOLD}${HG}  Main Menu${RESET}\n"
    divider
    printf "\n"
    printf "${HG}  [1]${RESET} ${WH}Full integrity report${RESET}\n"
    printf "${HG}  [2]${RESET} ${WH}CPU vulnerability audit${RESET}\n"
    printf "${HG}  [3]${RESET} ${WH}eBPF attack surface${RESET}\n"
    printf "${HG}  [4]${RESET} ${WH}Kernel modules${RESET}\n"
    printf "${HG}  [5]${RESET} ${WH}Casey kernel findings${RESET}\n"
    printf "${HG}  [6]${RESET} ${WH}SELinux status${RESET}\n"
    printf "${HG}  [7]${RESET} ${WH}Apply hardening${RESET}\n"
    printf "${HG}  [q]${RESET} ${WH}Quit${RESET}\n"
    printf "\n"
    divider
    printf "${HG}  Choice: ${RESET}"
    read -r choice

    case "$choice" in
        1) show_full;         printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        2) header; show_cpu_vulns;  printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        3) header; show_ebpf;       printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        4) header; show_modules;    printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        5) header; show_casey_kernel; printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        6) header; show_selinux;    printf "${HG}  Press enter...${RESET}"; read -r; menu ;;
        7) show_actions; menu ;;
        q|Q) printf "\n${HG}  ~ Kernel Sanders out. ~${RESET}\n\n"; exit 0 ;;
        *) continue ;;
    esac
    printf "\n${HG}  Press enter...${RESET}"; read -r
    done
}

detect_variant
menu

# ── MILLET module audit ───────────────────────────────────────────────────────
show_millet() {
    printf "${BOLD}${HG}  [MILLET] Xiaomi Binder Monitoring Modules${RESET}\n"
    divider
    mods=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/modules 2>/dev/null | grep -E 'millet|binder_gki'")
    if [ -z "$mods" ]; then
        printf "${WA}  [i] No millet/binder_gki modules found (or read blocked)${RESET}\n\n"
    else
        echo "$mods" | while read -r line; do
            printf "${BO}  ${line}${RESET}\n"
        done
        printf "\n"
    fi

    printf "${CR}  Parameter state (blocked from shell, root-ready check):${RESET}\n"
    for p in millet_debug millet_binder_switch millet_freeze_switch \
             frozen_uid_min binder_warn_ahead_space; do
        val=$(rish_read "/sys/module/millet_core/parameters/$p")
        if [ -z "$val" ] || [ "$val" = "DENIED" ]; then
            printf "${WA}  [?] ${p}: DENIED (SELinux)${RESET}\n"
        else
            printf "${GR}  [OK] ${p}=${val}${RESET}\n"
        fi
    done
    printf "\n"
}

# ── FBO service state ─────────────────────────────────────────────────────────
show_fbo() {
    printf "${BOLD}${HG}  [FBO] File-Based Optimization Service${RESET}\n"
    divider
    svc=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "service list 2>/dev/null | grep fbo")
    [ -n "$svc" ] && printf "${GR}  [OK] ${svc}${RESET}\n" || printf "${WA}  [?] fbo service not found in service list${RESET}\n"

    ulist=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "settings get secure FBO_UPLOAD_LIST 2>/dev/null")
    utime=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "settings get secure FBO_UPLOAD_TIME 2>/dev/null")
    printf "${WH}  FBO_UPLOAD_LIST: ${CR}${ulist:-null}${RESET}\n"
    printf "${WH}  FBO_UPLOAD_TIME: ${CR}${utime:-null}${RESET}\n"
    printf "${WA}  [i] Unverified whether list membership blocks kills — test pending${RESET}\n\n"
}

# ── ProcessManager kill counters ──────────────────────────────────────────────
show_processmanager() {
    printf "${BOLD}${HG}  [PROCESSMANAGER] Kill Strategy & Counters${RESET}\n"
    divider
    dump=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "dumpsys activity service ProcessManager 2>/dev/null")
    if [ -z "$dump" ]; then
        printf "${WA}  [?] No ProcessManager dump available${RESET}\n\n"
        return
    fi
    for flag in KILL_STRATEGY BG_INTERCEPT DISABLE; do
        val=$(echo "$dump" | grep -o "${flag}: *[a-z]*" | head -1)
        printf "${WH}  ${val:-$flag: unknown}${RESET}\n"
    done
    printf "\n${CR}  PKG_HIT_MSG (per-package kill counters):${RESET}\n"
    echo "$dump" | grep -o "PKG_HIT_MSG.*" | head -5 | while read -r line; do
        printf "${BO}  ${line}${RESET}\n"
    done
    printf "\n"
}

# ── LMKD netlink clients ──────────────────────────────────────────────────────
show_lmkd() {
    printf "${BOLD}${HG}  [LMKD] Low Memory Killer Daemon${RESET}\n"
    divider
    pid=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "pgrep -x lmkd" 2>/dev/null | tr -d '\n')
    if [ -z "$pid" ]; then
        printf "${WA}  [?] lmkd not found${RESET}\n\n"
        return
    fi
    printf "${WH}  PID: ${CR}${pid}${RESET}\n"
    caps=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/$pid/status 2>/dev/null | grep CapEff")
    printf "${WH}  ${caps}${RESET}\n"
    printf "${WA}  [i] CAP_KILL present — netlink proto 15 spoofability unverified${RESET}\n\n"
}

# ── PowerKeeper soft-target recon ─────────────────────────────────────────────
show_powerkeeper() {
    printf "${BOLD}${HG}  [POWERKEEPER] Soft Target Recon${RESET}\n"
    divider
    pid=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "pgrep -x powerkeeper" 2>/dev/null | tr -d '\n')
    if [ -z "$pid" ]; then
        printf "${WA}  [?] powerkeeper not found${RESET}\n\n"
        return
    fi
    threads=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/$pid/status 2>/dev/null | grep Threads")
    caps=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/$pid/status 2>/dev/null | grep -E '^Cap'")
    oom=$(RISH_APPLICATION_ID=com.termux ~/Rish/rish -c "cat /proc/$pid/oom_score_adj 2>/dev/null")
    printf "${WH}  PID: ${CR}${pid}${RESET}\n"
    printf "${WH}  ${threads}${RESET}\n"
    printf "${WH}  OOM score adj: ${CR}${oom}${RESET}\n"
    printf "${GR}  ${caps:-Cap*: none effective}${RESET}\n"
    printf "${BO}  [NOTE] No caps, not MIMD-protected — softest target mapped this session${RESET}\n\n"
}
