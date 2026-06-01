#!/data/data/com.termux/files/usr/bin/bash
# superhero_gaveld_bridge.sh
# Runs superhero and forwards any THREAT/WARN detections to gaveld ingest pipe

BASE="$HOME/MiuiserPeruser"
PIPE="$BASE/pipes/ingest.pipe"

bin/superhero 2>&1 | while IFS= read -r line; do
    echo "$line"  # passthrough to terminal/log

    # Forward threats to gaveld
    if echo "$line" | grep -qE "\[THREAT\]|\[WARN\]|HIDDEN_PROCESS|INTEGRITY:.*changed|Rootkit|hook detected"; then
        # Extract signal name
        signal="SUPERHERO_ANOMALY"
        echo "$line" | grep -q "HIDDEN_PROCESS"  && signal="SUPERHERO_HIDDEN_PROCESS"
        echo "$line" | grep -q "INTEGRITY"       && signal="SUPERHERO_INTEGRITY_THREAT"
        echo "$line" | grep -q "hook\|Hook"      && signal="SUPERHERO_HOOK_DETECTED"
        echo "$line" | grep -q "Rootkit"         && signal="SUPERHERO_ROOTKIT_DETECTED"
        echo "$line" | grep -q "injection"       && signal="SUPERHERO_INJECTION_DETECTED"

        # Emit to gaveld ingest pipe
        ctx=$(echo "$line" | sed 's/[][]/./g' | cut -c1-120)
        echo "superhero|${signal}|0.0|${ctx}" >> "$PIPE" 2>/dev/null
    fi
done
