#!/data/data/com.termux/files/usr/bin/bash
# analog_init.sh — brings turtlecomd on the air if it's not already running.
# Sourced once per Termux session via .bashrc; silent no-op if already up.
# "Roger that" — the base station checks in and signs on.

export MP_ON_AIR=1

BASE="$HOME/MiuiserPeruser"

# turtlecomd self-deduplicates via flock on its pidfile — no need to
# pre-check PID liveness here (stale/reused PIDs made that check unreliable).
# If one's already running, this attempt is a harmless no-op.
(cd "$BASE" && ./bin/turtlecomd >> logs/analog_init.log 2>&1 &)
