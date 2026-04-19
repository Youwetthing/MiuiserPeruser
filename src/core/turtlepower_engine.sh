#!/data/data/com.termux/files/usr/bin/bash

EVENT_FILE="$HOME/MiuiserPeruser/state/court.events"
RULE_FILE="$HOME/MiuiserPeruser/state/turtlepower.rules"
QUAR_FILE="$HOME/MiuiserPeruser/state/quarantine.state"

is_quarantined() {
    grep -q "^$1|" "$QUAR_FILE"
}

quarantine() {
    echo "$1|rule|$(date +%s)" >> "$QUAR_FILE"
    pkill -f "$1"
}

release() {
    grep -v "^$1|" "$QUAR_FILE" > "$QUAR_FILE.tmp" && mv "$QUAR_FILE.tmp" "$QUAR_FILE"
}

apply() {
    case "$2" in
        quarantine:*)
            quarantine "${2#quarantine:}"
            ;;
        release:*)
            release "${2#release:}"
            ;;
        restart:*)
            d="${2#restart:}"
            is_quarantined "$d" && exit 0
            pkill -f "$d"
            ;;
    esac
}

while IFS='|' read -r _ cond action; do
    key=$(echo "$cond" | cut -d'>' -f1 | cut -d'=' -f1)
    num=$(echo "$cond" | grep -o '[0-9]\+')

    count=$(grep "$key" "$EVENT_FILE" | wc -l)

    if [ "$count" -ge "$num" ]; then
        apply "$cond" "$action"
    fi
done < "$RULE_FILE"
