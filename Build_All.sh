#!/bin/bash
echo "🏰 [SYNDICATE HAUS] Final Structural Reinforcement..."
LAIRS="-I. -I./include -I./src/core/include -I./src/daemon"
LDFLAGS="-Wl,-z,muldefs"

mkdir -p bin pipes

echo "🥋 Training: IPC & Logic..."
gcc -c src/daemon/ipc.c $LAIRS -o bin/ipc.o
gcc -c src/daemon/ipc_globals.c $LAIRS -o bin/ipc_globals.o
gcc -c src/daemon/daemon_modes.c $LAIRS -o bin/modes.o
gcc -c src/daemon/doctor_mode.c $LAIRS -o bin/doctor_mode.o
gcc -c src/daemon/capabilities_extra.c $LAIRS -o bin/cap_extra.o
gcc -c src/daemon/capabilities_pretty.c $LAIRS -o bin/cap_pretty.o
gcc -c src/core/capabilities.c $LAIRS -o bin/capabilities.o
gcc -c src/core/sensei_core.c $LAIRS -o bin/sensei_core.o
gcc -c src/daemon/splinter_protocol.c $LAIRS -o bin/splinter_protocol.o

echo "⚡ Energizing miuiserperuser..."
gcc src/daemon/miuiserperuser.c \
    bin/*.o \
    src/daemon/daemon_core.c \
    src/daemon/help_extra.c \
    src/backend/backend_selector.c \
    src/backend/backend_rish.c \
    src/backend/backend_adb.c \
    $LAIRS $LDFLAGS -o bin/miuiserperuser

if [ $? -eq 0 ]; then
    chmod 700 bin/miuiserperuser
    echo "🔍 Forging the Mouser..."
    gcc src/backend/backend_doctor.c bin/*.o $LAIRS $LDFLAGS -o bin/mouser
    echo "✅ [SUCCESS] The Haus is standing. IPC is Hot."
else
    echo "❌ [ERROR] Still missing something. Check the symbol dump."
fi
