#!/bin/bash
SEWER="/data/data/com.termux/files/home/.syndicate_sewer"
mkdir -p "$SEWER"

echo "[SHADOW] Muscle Online. Listening at $SEWER"

while true; do
    # Check if the Question exists
    if [ -f "$SEWER/q.ready" ]; then
        # Atomic Read: Move it to a temp file so the Brain doesn't fight us
        mv "$SEWER/q.ready" "$SEWER/processing.tmp" 2>/dev/null
        
        if [ -f "$SEWER/processing.tmp" ]; then
            ACTION=$(cat "$SEWER/processing.tmp")
            rm -f "$SEWER/processing.tmp"
            
            if [ -z "$ACTION" ]; then ACTION="ping"; fi
            
            echo "[SHADOW] Brain said: $ACTION"
            
            # --- HYPEROS ACTIONS ---
            if [ "$ACTION" == "optimize_cores" ]; then
                echo "[SHADOW] EXECUTING: Performance Mode Toggle"
                # cmd power set-fixed-performance-mode-allowed true 2>/dev/null
            fi
            
            # Signal back to the Brain
            touch "$SEWER/a.ready"
        fi
    fi
    sleep 0.5
done
