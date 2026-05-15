#!/data/data/com.termux/files/usr/bin/bash
# MiuiserPeruser: Toolkit Intervention Daemon
# Actively executes system interventions received via IPC pipe

BASE="\$HOME/MiuiserPeruser"
IN_PIPE="\$BASE/pipes/execution.pipe"
LOG="\$BASE/logs/toolkit.log"

mkdir -p "\$BASE/logs"

# Use common lib if available
source "\$BASE/lib/miuiserperuser_common.sh" 2>/dev/null

log() {
    local ts=\$(date "+%Y-%m-%d %H:%M:%S")
    echo "[\$ts] [TOOLKIT] \$1" >> "\$LOG"
}

# Standard run_shell if library failed to load
if ! command -v run_shell >/dev/null; then
    run_shell() {
        if [[ -x "\$HOME/.shizuku/rish" ]]; then
            "\$HOME/.shizuku/rish" -c "\$*" 2>/dev/null
        elif command -v adb >/dev/null 2>&1; then
            adb shell "\$@" 2>/dev/null
        else
            return 1
        fi
    }
fi

declare -A SEEN
MAX_SEEN=500

log "ONLINE — Active Intervention Mode"

[ -p "\$IN_PIPE" ] || mkfifo "\$IN_PIPE"

while true; do
    if read line < "\$IN_PIPE"; then
        [[ -z "\$line" ]] && continue
        
        IFS="|" read -r action target ctx <<< "\$line"
        KEY="\$action|\$target|\$ctx"

        # IDEMPOTENCY GUARD & GC
        if [[ "\${SEEN[\$KEY]}" == "1" ]]; then
            continue
        fi
        
        if [[ \${#SEEN[@]} -gt \$MAX_SEEN ]]; then
            unset SEEN
            declare -A SEEN
        fi
        SEEN[\$KEY]=1

        case "\$action" in
            INTERVENE)
                log "SOFT_KILL \$target (ctx: \$ctx)"
                run_shell "am kill \$target"
                ;;

            ISOLATE)
                log "ISOLATE \$target (ctx: \$ctx)"
                run_shell "pm disable-user --user 0 \$target"
                ;;

            KILL)
                log "FORCE_STOP \$target (ctx: \$ctx)"
                run_shell "am force-stop \$target"
                ;;

            ESCALATE)
                log "ESCALATION_FORWARD \$target"
                [ -p "\$BASE/pipes/escalation.pipe" ] && echo "\$action|\$target|\$ctx" >> "\$BASE/pipes/escalation.pipe"
                ;;

            *)
                log "UNHANDLED \$action \$target"
                ;;
        esac
    fi
done
