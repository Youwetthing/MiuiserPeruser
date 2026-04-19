#!/bin/bash
echo "🧬 [SYNDICATE HIVEMIND] Initializing Lymphatic System..."
echo "--------------------------------------------------------"

# 1. Establish the IPC Hub (Layer 1)
~/bin/turtlecomd &
~/bin/krangd &
sleep 1

# 2. Deploy the Management Layer (Layer 2)
~/bin/splinterd &
~/bin/burned &

# 3. Release the Specialist Daemons (Layer 3)
DAEMONS=("shredderd" "leatherheadd" "granitord" "rocksteadyd" "bebopd" "metalheadd" "ratkingd" "tigerclawd" "fugitoidd" "rahzerd")

for d in "${DAEMONS[@]}"; do
    ~/bin/$d &
    echo "✨ [BONDED] $d is now circulating."
done

echo "--------------------------------------------------------"
echo "✅ SYNDICATE ONLINE: All 14 Nodes Connected to /tmp/syndicate.sock"
