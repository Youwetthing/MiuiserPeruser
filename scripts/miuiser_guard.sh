#!/usr/bin/env bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[miuiser-guard] Running self-healing checks from: $ROOT_DIR"

# 1) Ensure tmp directory exists (for sockets, etc.)
TMP_DIR="$ROOT_DIR/tmp"
if [ ! -d "$TMP_DIR" ]; then
  echo "[miuiser-guard] Creating tmp directory: $TMP_DIR"
  mkdir -p "$TMP_DIR"
fi

# 2) Ensure core/platform directory exists
PLATFORM_DIR="$ROOT_DIR/src/core/platform"
if [ ! -d "$PLATFORM_DIR" ]; then
  echo "[miuiser-guard] Creating platform directory: $PLATFORM_DIR"
  mkdir -p "$PLATFORM_DIR"
fi

# 3) Ensure rish_pipe stub exists if implementation is missing
HEADER="$ROOT_DIR/src/core/include/rish_pipe.h"
STUB="$PLATFORM_DIR/rish_pipe_stub.c"

if [ -f "$HEADER" ]; then
  # Look for any .c that defines rish_pipe_start
  if ! grep -R "rish_pipe_start" "$ROOT_DIR/src/core" --include='*.c' >/dev/null 2>&1; then
    echo "[miuiser-guard] No rish_pipe implementation found, ensuring stub at: $STUB"
    cat << 'STUBEOF' > "$STUB"
#include "rish_pipe.h"
#include <stdlib.h>

/*
 * Auto-generated stub by miuiser_guard.sh
 * Minimal implementations to satisfy the linker.
 */

int rish_pipe_start(void) {
    /* no-op */
    return 0;
}

void rish_pipe_stop(void) {
    /* no-op */
}

char *rish_pipe_command(const char *cmd) {
    (void)cmd;
    /* return NULL to indicate "no output" */
    return NULL;
}
STUBEOF
  else
    echo "[miuiser-guard] rish_pipe implementation already present; no stub needed."
  fi
else
  echo "[miuiser-guard] Warning: rish_pipe.h not found; skipping rish_pipe checks."
fi

echo "[miuiser-guard] All self-healing checks completed."
