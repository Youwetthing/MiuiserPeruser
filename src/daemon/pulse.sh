#!/bin/sh
# Single-fire sensor report
L=$(uptime | awk -F'load average:' '{print $2}' | cut -d, -f1 | xargs)
SAT=$(( (${L%.*} * 100) / 8 ))
echo "pressure=$SAT" > ~/.syndicate_sewer/rocksteady.packet
echo "[$(date +%T)] CPU SATURATION: $SAT%"
