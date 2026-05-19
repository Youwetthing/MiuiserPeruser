#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
EVT="$BASE/state/court.events"

echo "⚖️ APPEALS SYSTEM ACTIVE"

appeal() {
    target="$1"
    reason="$2"

    (flock -x 200; echo "$(date +%s)|APPEAL|$target|$reason" >> "$EVT") 200>"$EVT.lock"
    echo "⚖️ Appeal logged: $target"
}

# Example usage:
# appeal turtlepower "false crash detection"
