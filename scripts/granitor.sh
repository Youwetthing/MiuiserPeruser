#!/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
echo "[+] MiuiserPeruser Granitor Initializing..."
python3 brain.py &
./granite_muscle &
echo "[+] Both hemispheres active. Termux is now Sovereign."
