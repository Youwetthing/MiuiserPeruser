#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
IN_PIPE="$BASE/pipes/execution.pipe"
LOG="$BASE/logs/toolkit.log"

mkdir -p "$BASE/logs"

declare -A SEEN

log() {
    echo "[TOOLKIT] $1" >> "$LOG"
}

log "ONLINE"

[ -p "$IN_PIPE" ] || mkfifo "$IN_PIPE"

while true; do
    if read line < "$IN_PIPE"; then

        IFS="|" read -r action target ctx <<< "$line"

        KEY="$action|$target|$ctx"

        # IDEMPOTENCY GUARD
        if [[ "${SEEN[$KEY]}" == "1" ]]; then
            log "SKIP_DUPLICATE $KEY"
            continue
        fi
        SEEN[$KEY]=1

        case "$action" in

            INTERVENE)
                log "SOFT_LIMIT $target"
                ;;

            ISOLATE)
                log "ISOLATE $target"
                ;;

            KILL)
                log "KILL $target"
                ;;

            ESCALATE)
                log "ESCALATION_FORWARD $target"
                echo "$action|$target|$ctx" >> "$BASE/pipes/escalation.pipe"
                ;;

            *)
                log "UNKNOWN_ACTION $action"
                ;;

        esac

    fi
done
