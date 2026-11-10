#!/data/data/com.termux/files/usr/bin/bash
# install.sh - install build toolchain and create directories.
# Does NOT build, start daemons, call rish, or create consent markers.
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ "$BASE" = "$HOME/MiuiserPeruser" ] || echo "WARN: expected $HOME/MiuiserPeruser, found $BASE (some paths are hardcoded)"

pkg install -y clang cmake make openssl || { echo "pkg install failed"; exit 1; }
for c in clang cmake make; do
    command -v "$c" >/dev/null 2>&1 || { echo "missing after install: $c"; exit 1; }
done

mkdir -p "$BASE"/logs "$BASE"/state "$BASE"/data/tmp \
         "$BASE"/pipes/pids "$BASE"/pipes/state "$BASE"/pipes/daemon_results
chmod 700 "$BASE"/pipes
chmod +x "$BASE"/start_fleet.sh "$BASE"/scripts/*.sh 2>/dev/null

[ -x "$HOME/rish" ] || echo "NOTE: ~/rish not found (export it from Shizuku)"
[ -x "$HOME/.cargo/bin/adb_cli" ] || echo "NOTE: ~/.cargo/bin/adb_cli not found (adb fallback unavailable)"
echo "Optional (dashboards/tools): pkg install jq sqlite python"
echo "Done. Build: mkdir -p build && cd build && cmake .. && make -j4"
echo "Then: bash start_fleet.sh"
