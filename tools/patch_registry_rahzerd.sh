#!/data/data/com.termux/files/usr/bin/bash

REG="~/MiuiserPeruser/Registry/system_registry.json"

echo "[+] Applying Rahzerd consolidation patch..."

sed -i 's/connectivityd/rahzerd/g' "$REG"

# remove duplicates if both logs exist
grep -q "rahzerd.log" "$REG" && sed -i '/connectivityd.log/d' "$REG"

echo "[✓] Registry unified under rahzerd"
