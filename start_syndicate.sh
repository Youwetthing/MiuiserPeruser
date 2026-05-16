#!/bin/bash
# start_syndicate.sh — launch the full syndicate daemon fleet
# Run from anywhere; resolves binaries relative to MiuiserPeruser base.

BASE="${BASE:-$HOME/MiuiserPeruser}"
BIN="$BASE/bin"

# Ensure required directories exist before any daemon starts
mkdir -p "$BASE/pipes/pids"
mkdir -p "$BASE/pipes/state"   # needed by rocksteadyd.last (Step 3)
mkdir -p "$BASE/logs"

echo "[SYNDICATE] Initialising daemon fleet..."
echo "---------------------------------------------------"

# ── Tier 1: IPC Hub (must be up before reporters connect) ──────────────
echo "[SYNDICATE]  Tier 1 — IPC hub"
# Redirect turtlecomd stdout to log file — court_dispatcher tails this
"$BIN/turtlecomd" >> "$BASE/logs/turtlecomd.log" 2>&1 &
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

# ── Tier 4: Judicial system ────────────────────────────────────────────
echo "[SYNDICATE]  Tier 4 — Judicial system"
bash "$BASE/law_and_order:adb/judicial_controller.sh" start

# court_dispatcher bridges turtlecomd APRIL stream → judicial pipe
nohup bash "$BASE/law_and_order:adb/court_dispatcher.sh" \
    >> "$BASE/logs/court_dispatcher.log" 2>&1 &
echo "[SYNDICATE]   + court_dispatcher (PID $!)"

echo "---------------------------------------------------"
echo "[SYNDICATE] Fleet deployed — $(( ${#DAEMONS[@]} + 4 )) processes launched"
echo "[SYNDICATE] IPC hub : $BASE/pipes/turtlecom.sock"
echo "[SYNDICATE] Krang   : $BASE/pipes/krang.sock"
echo "[SYNDICATE] Splinter: $BASE/pipes/splinterd.sock"
echo "[SYNDICATE] Judicial: $BASE/law_and_order:adb/"
