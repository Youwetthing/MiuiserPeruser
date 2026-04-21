#!/data/data/com.termux/files/usr/bin/bash

FILE="src/core/include/april_core.h"

sed -i 's/APRIL_POLL_NORMAL/APRIL_POLL_NORMAL_E/g' "$FILE"
sed -i 's/APRIL_POLL_THROTTLED/APRIL_POLL_THROTTLED_E/g' "$FILE"
sed -i 's/APRIL_POLL_MINIMAL/APRIL_POLL_MINIMAL_E/g' "$FILE"

sed -i 's/APRIL_SYSTEM_LOCK/APRIL_SYSTEM_LOCK_E/g' "$FILE"

sed -i 's/APRIL_LOG_LEVEL = 1/APRIL_LOG_LEVEL_E = 1/g' "$FILE"
sed -i 's/APRIL_LOG_LEVEL = 2/APRIL_LOG_LEVEL_E = 2/g' "$FILE"
