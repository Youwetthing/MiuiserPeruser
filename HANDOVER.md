# MiuiserPeruser — Handover v3
*Next Claude instance read this first.*

## System Status: OPERATIONAL
8 daemons running. Full pipeline verified with live device signals.

## Verified Pipeline
Real device battery sensor (20% health)
→ superhero binary (rish backend, ARM64)
→ scripts/superhero_adapter.sh (parses stderr, deduplicates per scan)
→ pipes/superhero.pipe
→ april_o_neil (case assembly, 500 cap)
→ pipes/judgement.pipe
→ judge_executor (verdict)
→ state/criminal_record/ledger.log ✅
→ state/court.events ✅
→ state/jailhouse/registry (if JAILED) ✅
→ pipes/execution.pipe → baxter_stockman (NOT YET WIRED)

## Start Command
bash ~/MiuiserPeruser/law_and_order:adb/judicial_controller.sh start

## 8 Managed Daemons
court_core_engine, judge_executor, court_orchestrator, escalation,
april_o_neil, visitors_pass, turtlepower, superhero (adapter)

## Priority 1 — Baxter Stockman (enforcement arm)
File: scripts/toolkit_daemon.sh → rename to scripts/baxter_stockman.sh
Issues:
- Escaped \$ variables throughout (heredoc artifact) — full rewrite needed
- FIFO busy loop — fix with exec 3<>
- Not in judicial_controller DAEMON_SCRIPTS yet
- execution.pipe exists but nobody writes to it

Wire judge_executor.sh → execution.pipe after each verdict:
  score ≥ 80  → echo "KILL|$src|score=$score" > $BASE/pipes/execution.pipe
  score 50-79 → echo "ISOLATE|$src|score=$score" > $BASE/pipes/execution.pipe
  score < 50  → echo "INTERVENE|$src|score=$score" > $BASE/pipes/execution.pipe

Baxter actions:
  KILL      → rish: am force-stop $target
  ISOLATE   → rish: pm disable-user --user 0 $target
  INTERVENE → rish: am kill $target

## Priority 2 — Git merge conflict
git checkout --ours CMakeLists.txt Makefile \
    src/daemon/bebopd.c src/daemon/ipc.c src/daemon/krangd.c \
    src/daemon/daemon_core.c src/daemon/capabilities_extra.c \
    src/daemon/splinter_protocol.h daemons/powerhouse.py flip_switch.py
git add .
git commit -m "resolve merge conflicts"
Then: git add law_and_order:adb/ src/core/backend/ src/core/turtlepower_engine.sh
git commit -m "feat: judicial system complete + superhero adapter live"

## Priority 3 — Superhero adapter known issues
- SyntaxWarning on \[ in regex — fix with raw strings or double backslash
- EMITTED guard uses 'continue' inside while+pipe subshell — may not work
  correctly in bash. Test with: declare -A inside pipe subshell loses state.
  Fix: run superhero binary to a temp file, parse file, not a pipe.
- Signals not yet detected by adapter (need real scan to trigger):
  CPU_THROTTLING, RWX_MEMORY_PAGE, INTEGRITY_VIOLATION, NETWORK_ANOMALY

## Priority 4 — Registry
Add to Registry/daemon_allowlist.json:
"judicial": [court_core_engine, court_orchestrator, april_o_neil,
             escalation, visitors_pass, turtlepower, judge_executor]
"enforcement": [baxter_stockman]
"superhero": [superherod, leo, casey, mikey, don, raph]
Fix compile_registry.sh — expects flat list, now categorised JSON.

## Priority 5 — judicial_firewall.sh
Add QUARANTINE to allowed actions:
RESTART|ISOLATE|THROTTLE|KILL|WIPE|FORMAT|QUARANTINE

## Priority 6 — Unreviewed files
cat law_and_order:adb/judicial_appeals.sh
cat law_and_order:adb/judge_judy.sh
cat law_and_order:adb/cre_case_ingestor.sh

## Key Paths
BASE = ~/MiuiserPeruser
LAW  = $BASE/law_and_order:adb
PIPE = $BASE/pipes/superhero.pipe
EXEC = $BASE/pipes/execution.pipe
EVT  = $BASE/state/court.events
REG  = $BASE/state/court.registry
LEDGER = $BASE/state/criminal_record/ledger.log
ROCKY  = $BASE/pipes/state/rocksteadyd.last (syndicate only)

## Superhero Binary
Location: Superhero_Mode/superhero (69KB, ARM64, NDK r29)
Run modes:
  superhero            → one scan
  superhero --loop 30  → continuous 30s interval
  superhero --n 5 10   → 5 scans, 10s interval
Backend: forces RISH, connects to /data/local/tmp/portbridge.sock
Emits: APRIL_EVENT_JSON to socket (not readable from Termux userspace)
Adapter parses stderr instead — works correctly.

## Known Signal Scores
BATTERY_HEALTH_LOW  → 75  → QUARANTINED (battery ≤ 20%)
BATTERY_HEALTH_LOW  → 45  → QUARANTINED (battery ≤ 40%)
CPU_THROTTLING      → 60  → QUARANTINED
RWX_MEMORY_PAGE     → 90  → JAILED
NETWORK_ANOMALY     → 70  → QUARANTINED
HIDDEN_PROCESS      → 85  → JAILED
INTEGRITY_VIOLATION → 95  → JAILED
MIUI_GAME_TURBO     → 50  → QUARANTINED

## Portbridge (future)
/data/local/tmp/portbridge.sock — server not running yet
socat available — once portbridge server exists, adapter can switch to:
socat UNIX-CONNECT:/data/local/tmp/portbridge.sock STDOUT | parse JSON
This gives proper structured APRIL_EVENT_JSON instead of stderr parsing.
