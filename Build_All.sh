#!/usr/bin/env bash
# ═══════════════════════════════════════════════════════════════
#  Build_All.sh — MiuiserPeruser Syndicate Layer Build Script
#  Run from the repo root on-device (Termux / Android)
#  Usage:  bash Build_All.sh [--verbose] [--clean]
# ═══════════════════════════════════════════════════════════════

set -euo pipefail

# ── Flags ──────────────────────────────────────────────────────
VERBOSE=false
CLEAN=false
for arg in "$@"; do
    case "$arg" in
        --verbose|-v) VERBOSE=true ;;
        --clean|-c)   CLEAN=true  ;;
    esac
done

# ── Paths ──────────────────────────────────────────────────────
REPO="$(cd "$(dirname "$0")" && pwd)"
DAEMON="$REPO/src/daemon"
CORE="$REPO/src/core"
BIN="$REPO/bin"

# ── Compiler detection ─────────────────────────────────────────
if   command -v clang   >/dev/null 2>&1; then CC=clang
elif command -v gcc     >/dev/null 2>&1; then CC=gcc
elif command -v cc      >/dev/null 2>&1; then CC=cc
else echo "ERROR: no C compiler found — install clang: pkg install clang" >&2; exit 1
fi

# ── Colours ────────────────────────────────────────────────────
GRN=$(printf '\033[32m'); RED=$(printf '\033[31m')
YLW=$(printf '\033[33m'); BLU=$(printf '\033[34m')
RST=$(printf '\033[0m');  BLD=$(printf '\033[1m')

# ── Compile flags ──────────────────────────────────────────────
# -fPIE / -pie:  position-independent executable (required for Android)
# -std=c11:      for _exit, weak attributes, etc.
# -Wno-unused-*: avoid noise from stubs
CFLAGS="-O2 -std=c11 -fPIE -pie -Wall -Wextra -Wno-unused-parameter -Wno-unused-function"
INC="-I$DAEMON -I$CORE"

# Sources shared by every daemon binary
SHARED="$DAEMON/daemon_core.c $DAEMON/ipc_globals.c $DAEMON/daemon_common.c $DAEMON/backend_exec.c"

# ── Helpers ────────────────────────────────────────────────────
PASS=0; FAIL=0; FAIL_LIST=()

build() {
    local name="$1"; shift
    local srcs="$*"
    local out="$BIN/$name"
    local log; log=$(mktemp)

    printf "  %-18s " "$name"

    if $VERBOSE; then
        echo ""
        echo "    ${YLW}$CC $CFLAGS $INC $srcs -o $out${RST}"
    fi

    # shellcheck disable=SC2086
    if $CC $CFLAGS $INC $srcs -o "$out" >"$log" 2>&1; then
        echo "${GRN}✓${RST}"
        PASS=$(( PASS + 1 ))
    else
        echo "${RED}✗  FAILED${RST}"
        cat "$log"
        FAIL=$(( FAIL + 1 ))
        FAIL_LIST+=("$name")
    fi
    rm -f "$log"
}

# ═══════════════════════════════════════════════════════════════
echo ""
echo "${BLD}${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RST}"
echo "${BLD}  MiuiserPeruser — Syndicate Daemon Build${RST}"
echo "${BLD}${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RST}"
echo "  Compiler : $CC"
echo "  Repo     : $REPO"
echo "  Output   : $BIN/"
echo ""

# ── Pre-flight ─────────────────────────────────────────────────
mkdir -p "$BIN"

# Optional clean: remove old syndicate binaries before rebuild
if $CLEAN; then
    echo "${YLW}  Cleaning old binaries...${RST}"
    for d in turtlecomd krangd splinterd burned granitord shredderd \
              bebopd fugitoidd leatherheadd metalheadd rocksteadyd ratkingd rahzerd; do
        rm -f "$BIN/$d"
    done
fi

echo "${BLD}  Building...${RST}"
echo ""

# ── Tier 1 — IPC Hub ───────────────────────────────────────────
echo "  ${YLW}[Tier 1]${RST} IPC hub"

#  turtlecomd — central broadcast hub; all reporters connect here
build turtlecomd \
    "$DAEMON/turtlecomd.c" \
    $SHARED

#  krangd — normalise payloads through truth_engine, push to dashboard
build krangd \
    "$DAEMON/krangd.c" \
    "$DAEMON/krang.c" \
    "$CORE/truth_engine.c" \
    $SHARED

#  splinterd — APRIL event bus (routes events between daemons)
build splinterd \
    "$CORE/splinterd.c" \
    $SHARED

echo ""
# ── Tier 2 — Policy / Security reporters (→ turtlecom) ────────
echo "  ${YLW}[Tier 2]${RST} Policy & security reporters"

#  burned — MIUI/HyperOS property watcher
build burned \
    "$DAEMON/burned.c" \
    $SHARED

#  granitord — boot-state, SELinux, encryption, root check
build granitord \
    "$DAEMON/granitord.c" \
    $SHARED

#  shredderd — kernel integrity: verified boot, Magisk, flash lock
build shredderd \
    "$DAEMON/shredderd.c" \
    $SHARED

echo ""
# ── Tier 3 — Standalone monitors (daemon_core lifecycle) ───────
echo "  ${YLW}[Tier 3]${RST} Standalone monitors"

#  bebopd — wake locks & pending alarms
build bebopd \
    "$DAEMON/bebopd.c" \
    $SHARED

#  fugitoidd — RISH/ADB bridge, log stream & activity monitor
build fugitoidd \
    "$DAEMON/fugitoidd.c" \
    $SHARED

#  leatherheadd — real skin temperature via thermalservice
build leatherheadd \
    "$DAEMON/leatherheadd.c" \
    $SHARED

#  metalheadd — sensor service scanner
build metalheadd \
    "$DAEMON/metalheadd.c" \
    $SHARED

#  rocksteadyd — CPU frequency + throttle detection
build rocksteadyd \
    "$DAEMON/rocksteadyd.c" \
    $SHARED

#  ratkingd — zombie process & CPU-hog hunter
build ratkingd \
    "$DAEMON/ratkingd.c" \
    $SHARED

echo ""
# ── Tier 4 — Full-spectrum connectivity ────────────────────────
echo "  ${YLW}[Tier 4]${RST} Full-spectrum connectivity"

#  rahzerd v2 — 15-layer connectivity audit, emits to splinterd
#  getaddrinfo lives in libc on Android/Termux — no extra -l needed
build rahzerd \
    "$DAEMON/rahzerd.c" \
    $SHARED \
    -lpthread

# ── Summary ────────────────────────────────────────────────────
echo ""
echo "${BLD}${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RST}"
TOTAL=$(( PASS + FAIL ))
if [ $FAIL -eq 0 ]; then
    echo "  ${GRN}${BLD}✓ All $TOTAL targets built successfully${RST}"
    echo ""
    echo "  To start the fleet:"
    echo "    ${BLU}bash $REPO/scripts/start_syndicate.sh${RST}"
else
    echo "  ${GRN}$PASS/$TOTAL${RST} built — ${RED}${BLD}$FAIL failed:${RST} ${FAIL_LIST[*]}"
fi
echo "${BLD}${BLU}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${RST}"
echo ""

[ $FAIL -eq 0 ]   # exit 0 on success, 1 on any failure
