#!/data/data/com.termux/files/usr/bin/bash
# install.sh - clone to ~/MiuiserPeruser, run once, then build and start_fleet.sh
BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
mkdir -p "$BASE"/logs "$BASE"/state "$BASE"/data/tmp \
         "$BASE"/pipes/pids "$BASE"/pipes/state "$BASE"/pipes/daemon_results
chmod +x "$BASE"/start_fleet.sh "$BASE"/scripts/*.sh 2>/dev/null
echo "Dirs ready. Build: mkdir -p build && cd build && cmake .. && make -j4. Then: bash start_fleet.sh"
