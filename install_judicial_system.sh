#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
LAW="$BASE/law_and_order:adb"
STATE="$BASE/state"

echo "⚖️ Judicial System Bootstrap Starting..."

# ── Pipes ────────────────────────────────────────────
mkdir -p "$BASE/pipes/state" "$BASE/pipes/pids"
for p in superhero.pipe judgement.pipe execution.pipe escalation.pipe; do
    [ -p "$BASE/pipes/$p" ] || mkfifo "$BASE/pipes/$p"
done
echo "✔ Pipes ready"

# ── State directories & seed files ───────────────────
mkdir -p "$STATE/criminal_record" \
         "$STATE/jailhouse" \
         "$STATE/visitors_pass" \
         "$BASE/cre/cases" \
         "$BASE/logs"

[ -f "$STATE/court.registry" ]   || echo "# NAME|STATE|PID" > "$STATE/court.registry"
[ -f "$STATE/court.events" ]     || touch "$STATE/court.events"
[ -f "$STATE/quarantine.state" ] || echo "# daemon|reason|timestamp" > "$STATE/quarantine.state"
[ -f "$STATE/cases.state" ]      || touch "$STATE/cases.state"

# CRITICAL: turtlepower won't start without this
[ -f "$STATE/turtlepower.lock" ] || echo "LOCK_STATE=ACTIVE" > "$STATE/turtlepower.lock"

echo "✔ State initialised"

# ── Permissions ───────────────────────────────────────
chmod +x "$LAW"/*.sh 2>/dev/null
chmod +x "$LAW/cre"/*.sh 2>/dev/null
chmod +x "$BASE/scripts"/*.sh 2>/dev/null
echo "✔ Permissions set"

echo "⚖️ Judicial System Fully Installed"

if [ "$1" = "start" ]; then
    echo "⚖️ Court is being convened..."
    bash "$LAW/judicial_controller.sh" start
fi
