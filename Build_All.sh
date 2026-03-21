#!/data/data/com.termux/files/usr/bin/bash
# --- Shortcut flags ---
FAST=false
VERBOSE=false
NOCOLOR=false
WATCH=false

for arg in "$@"; do
    case "$arg" in
        --fast) FAST=true ;;
        --verbose) VERBOSE=true ;;
        --no-color) NOCOLOR=true ;;
        --watch) WATCH=true ;;
    esac
done

# Disable colors if requested
if [ "$NOCOLOR" = true ]; then
    RED=""; GRN=""; YLW=""; BLU=""; RST=""
fi

set -euo pipefail

REPO_ROOT=~/MiuiserPeruser
EXEC=src/daemon/miuiserperuser

# Colors (fallback if unsupported)
RED=$(printf '\033[31m')
GRN=$(printf '\033[32m')
YLW=$(printf '\033[33m')
BLU=$(printf '\033[34m')
RST=$(printf '\033[0m')

echo "=============================="
echo "🛠 ${BLU}Starting self-healing TMNT build${RST}"
echo "=============================="

cd "$REPO_ROOT"

# ----------------------------------------
# 1) Verify repo structure
# ----------------------------------------
echo "🔍 Verifying repo structure..."

if [ ! -f "./CMakeLists.txt" ] || [ ! -d "./src/daemon" ]; then
    echo "❌ ${RED}Not in MiuiserPeruser repo root${RST}"
    exit 1
fi

required_files=(
    "src/daemon/daemon_common.c"
    "src/daemon/service.c"
    "src/daemon/rish_pipe.c"
)

for f in "${required_files[@]}"; do
    if [ ! -f "$f" ]; then
        echo "❌ ${RED}Missing required file:${RST} $f"
        exit 1
    fi
done

echo "✔ ${GRN}Repo structure OK${RST}"

# ----------------------------------------
# 2) Auto-fix misnamed files
# ----------------------------------------
echo "🩹 Checking for misnamed files..."

declare -A renames=(
    ["src/daemon/miuixiaomi.c"]="src/daemon/miui_xiaomi.c"
    ["src/daemon/daemoncommon.c"]="src/daemon/daemon_common.c"
)

for wrong in "${!renames[@]}"; do
    right="${renames[$wrong]}"
    if [ -f "$wrong" ]; then
        mv "$wrong" "$right"
        echo "✔ Renamed ${YLW}$wrong${RST} → ${GRN}$right${RST}"
    fi
done

# ----------------------------------------
# 3) Clean build
# ----------------------------------------
echo "🧹 Cleaning build directory..."
rm -rf build
mkdir build && cd build

echo "⚙️ Running CMake..."
cmake .. >/dev/null

echo "🔨 Building..."
make -j"$(nproc)" >/dev/null

echo "🎉 ${GRN}Build finished${RST}"

# ----------------------------------------
# 4) Verify symbols
# ----------------------------------------
echo "🔍 Verifying critical symbols in $EXEC"

if [ ! -x "$EXEC" ]; then
    echo "❌ ${RED}Executable missing:${RST} $EXEC"
    exit 1
fi

symbols=(
    miuiserperuser_service_start
    rish_pipe_start
    rish_pipe_stop
    rish_pipe_command
)

all_ok=true

for s in "${symbols[@]}"; do
    if nm -g --defined-only "$EXEC" | grep -q "$s"; then
        echo "✔ ${GRN}$s${RST}"
    else
        echo "❌ ${RED}Missing:${RST} $s"
        all_ok=false
    fi
done

# ----------------------------------------
# 5) Final summary
# ----------------------------------------
echo "=============================="

if [ "$all_ok" = true ]; then
    echo "✅ ${GRN}TMNT daemon build completed successfully!${RST}"
else
    echo "❌ ${RED}Build completed but some symbols are missing.${RST}"
fi

echo "=============================="
