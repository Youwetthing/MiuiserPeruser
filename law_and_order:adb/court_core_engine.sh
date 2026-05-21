#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"
MAX_LOG_BYTES=524288  # 500KB

_rotate_log() {
    local f="${1:-$LOG}"
    if [ -f "$f" ] && [ "$(stat -c%s "$f" 2>/dev/null || echo 0)" -gt "${MAX_LOG_BYTES:-524288}" ]; then
        mv "$f" "${f}.1"
        > "$f"
    fi
}


BASE="$BASE"
EVT="$BASE/state/court.events"
JAIL="$BASE/state/jailhouse"

mkdir -p "$JAIL"

echo "⚖️ COURT CORE ENGINE ONLINE (CLEAN REWRITE MODE)"

# -------------------------
# FILTER LOGIC (SAFE SCOPE)
# -------------------------
should_ignore_case() {
    local text="$1"
    echo "$text" | grep -qiE "test|binding|routing"
}

is_already_jailed() {
    local name="$1"
    grep -q "^$name|JAIL" "$JAIL/registry" 2>/dev/null
}

apply_jail() {
    local name="$1"
    local reason="$2"

    is_already_jailed "$name" && return

    echo "$(date +%s)|CORE|JAIL|$name:$reason" >> "$EVT"
    echo "$name|JAIL|$reason|$(date +%s)" >> "$JAIL/registry"
}

# -------------------------
# MAIN LOOP
# -------------------------
while true; do
    while IFS='|' read -r ts src type payload; do

        [[ "$src" != "CRE" ]] && continue
        [[ "$type" != "CASE" ]] && continue

        IFS=':' read -r target severity detail <<< "$payload"

        should_ignore_case "$detail" && continue

        if [ "$severity" = "HIGH" ]; then
            apply_jail "$target" "$detail"
        elif [ "$severity" = "MEDIUM" ]; then
            echo "$(date +%s)|CORE|INFO|RECORD:$target:$detail" >> "$EVT"
        fi

    done < "$EVT"

    sleep 3
done
