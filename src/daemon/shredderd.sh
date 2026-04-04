#!/bin/bash
# SHREDDERD: The Executioner

SEWER="/data/data/com.termux/files/home/.syndicate_sewer"

while true; do
    # Check for the Brain's Kill Order (q.ready)
    if [ -f "$SEWER/q.ready" ]; then
        TARGET=$(cat "$SEWER/q.ready")
        
        # Log the kill to Fugitoidd's ledger
        echo "[SHREDDER] Reaping target: $TARGET" >> ~/syndicate.history
        
        # The actual reap (using rish for high-level permissions)
        rish -c "pkill -f $TARGET"
        
        # Clean up the order
        rm "$SEWER/q.ready"
    fi
    sleep 2
done
