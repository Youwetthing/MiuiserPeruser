#!/data/data/com.termux/files/usr/bin/bash

LOGDIR=~/MiuiserPeruser/Log_Cabin

if [ -f "$LOGDIR/connectivityd.log" ]; then
  echo "[+] Merging legacy connectivityd logs into rahzerd"
  cat "$LOGDIR/connectivityd.log" >> "$LOGDIR/rahzerd.log"
  mv "$LOGDIR/connectivityd.log" "$LOGDIR/rahzerd_legacy.log"
fi

echo "[✓] Log merge complete"
