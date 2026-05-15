#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
EVT="$BASE/state/court.events"

echo "⚖️ APPEALS SYSTEM ACTIVE"

appeal() {
    target="$1"
    reason="$2"

    echo "$(date +%s)|APPEAL|$target|$reason" >> "$EVT"
    echo "⚖️ Appeal logged: $target"
}

# Example usage:
# appeal turtlepower "false crash detection"
