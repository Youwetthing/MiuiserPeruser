#!/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}

# start_syndicate.sh — launch the full syndicate daemon fleet
# Run from anywhere; resolves binaries relative to MiuiserPeruser base.

BASE="${BASE:-$BASE}"
BIN="$BASE/bin"

# Ensure the pipes and pids directories exist before any daemon starts
mkdir -p "$BASE/pipes/pids" "$BASE/pipes/state"
mkdir -p "$BASE/logs"

echo "[SYNDICATE] Initialising daemon fleet..."
echo "---------------------------------------------------"

# ── Tier 1: IPC Hub (must be up before reporters connect) ──────────────
echo "[SYNDICATE]  Tier 1 — IPC hub"
"$BIN/turtlecomd" &
"$BIN/krangd"     &
sleep 1

# ── Tier 2: Event bus + MIUI policy watcher ────────────────────────────
echo "[SYNDICATE]  Tier 2 — event bus / policy"
"$BIN/splinterd" &
"$BIN/burned"    &
sleep 1

# ── Tier 3: Specialist monitors ────────────────────────────────────────
echo "[SYNDICATE]  Tier 3 — specialist daemons"
DAEMONS=(
    granitord
    shredderd
    leatherheadd
    rocksteadyd
    bebopd
    metalheadd
    ratkingd
    fugitoidd
    rahzerd
)

for d in "${DAEMONS[@]}"; do
    if [ -x "$BIN/$d" ]; then
        "$BIN/$d" &
        echo "[SYNDICATE]   + $d"
    else
        echo "[SYNDICATE]   ! $d not found at $BIN/$d — skipping"
    fi
done

echo "---------------------------------------------------"
echo "[SYNDICATE] Fleet deployed — $(( ${#DAEMONS[@]} + 4 )) processes launched"
echo "[SYNDICATE] IPC hub : $BASE/pipes/turtlecom.sock"
echo "[SYNDICATE] Krang   : $BASE/pipes/krang.sock"
echo "[SYNDICATE] Splinter: $BASE/pipes/splinterd.sock"

# ── Tier 4: Judicial system ────────────────────────────────────────────
echo "[SYNDICATE]  Tier 4 — Judicial system"
bash "$BASE/law_and_order:adb/judicial_controller.sh" start

nohup bash "$BASE/law_and_order:adb/court_dispatcher.sh" \
    >> "$BASE/logs/court_dispatcher.log" 2>&1 &

CDPID=$!
echo "$CDPID" > "$BASE/pipes/pids/court_dispatcher.pid"
echo "[SYNDICATE]   + court_dispatcher (PID $CDPID)"
echo "[SYNDICATE] Judicial: $BASE/law_and_order:adb/"

