#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
PIPES="$BASE/pipes"
LOGS="$BASE/Log_Cabin"
BOOT_DIR="$HOME/.termux/boot"
BOOT_SCRIPT="$BOOT_DIR/start_miuiser.sh"

echo "[INSTALL] MiuiserPeruser setup starting..."

mkdir -p "$PIPES" "$LOGS" "$BASE/Database" "$BASE/bin"
chmod 700 "$PIPES"

[ ! -d "$PIPES" ] && echo "[INSTALL] FATAL: pipes/ failed." && exit 1
[ ! -d "$LOGS"  ] && echo "[INSTALL] FATAL: Log_Cabin/ failed." && exit 1

echo "[INSTALL] Directory structure OK."
echo "[INSTALL] Building binaries..."

gcc "$BASE/src/syndicate/krangd.c" "$BASE/src/core/april_table.c" \
    -I "$BASE/src/syndicate" -I "$BASE/src/core" -lsqlite3 -o "$BASE/bin/krangd" \
    || { echo "[INSTALL] FATAL: krangd build failed."; exit 1; }

gcc "$BASE/src/syndicate/turtlecomd.c" "$BASE/src/core/april_table.c" \
    -I "$BASE/src/syndicate" -I "$BASE/src/core" -lsqlite3 -o "$BASE/bin/turtlecomd" \
    || { echo "[INSTALL] FATAL: turtlecomd build failed."; exit 1; }

gcc "$BASE/src/toolkit/dex_ping.c" \
    -I "$BASE/src/syndicate" -o "$BASE/bin/dex_ping" \
    || { echo "[INSTALL] FATAL: dex_ping build failed."; exit 1; }

echo "[INSTALL] Binaries OK."

ln -sf "$BASE/bin/miuiser.sh" "$PREFIX/bin/miuiser"
chmod +x "$BASE/bin/miuiser.sh"
echo "[INSTALL] miuiser command available."

if [ -d "$BOOT_DIR" ]; then
    cp "$BASE/termux_boot/start_miuiser.sh" "$BOOT_SCRIPT"
    chmod +x "$BOOT_SCRIPT"
    echo "[INSTALL] Boot entry installed."
else
    echo "[INSTALL] termux-boot not found — session mode only."
fi

echo "[INSTALL] Bringing daemons online..."
miuiser start
echo "[INSTALL] Done."

# Initialise april.bin if missing
python3 -c "
import os
p = '$BASE/Database/april.bin'
if not os.path.exists(p):
    open(p, 'wb').write(b'\x00' * 4096)
    print('[INSTALL] april.bin initialised.')
else:
    print('[INSTALL] april.bin exists.')
"
