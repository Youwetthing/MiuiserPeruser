#!/data/data/com.termux/files/usr/bin/bash

echo "[+] Checking for forbidden daemon alias: connectivityd"

if grep -R "connectivityd" ~/MiuiserPeruser >/dev/null 2>&1; then
  echo "[X] FAILURE: connectivityd still exists in repo"
  grep -R "connectivityd" ~/MiuiserPeruser
  exit 1
fi

echo "[✓] Clean: Rahzerd fully canonical"
