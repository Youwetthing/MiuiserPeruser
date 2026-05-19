#!/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
BASE_DIR="$BASE"
PIPES="$BASE_DIR/pipes"

echo "--- Draining the Sewer ---"
pkill -9 turtlecom_live
pkill -9 krang_live
pkill -9 splinter_live
rm -rf "$PIPES"
mkdir -p "$PIPES"

echo "--- Initializing Heart (Hub) ---"
"$BASE_DIR/turtlecom_live" &
while [ ! -S "$PIPES/turtlecom.sock" ]; do sleep 0.5; done

echo "--- Initializing Muscle (Krang) ---"
"$BASE_DIR/krang_live" &

echo "--- Initializing Brain (Splinter) ---"
"$BASE_DIR/splinter_live" &

echo "--- Amazing Grace: Sewer is Live ---"
