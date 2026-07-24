#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

cd "$(dirname "$0")/build" || exit 1
./storaged >> tigerclaw.log 2>&1 &
echo $! > tigerclaw.pid
echo "TigerClaw started as PID $(cat tigerclaw.pid)"
