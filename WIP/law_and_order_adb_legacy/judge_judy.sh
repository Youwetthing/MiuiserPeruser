#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

BASE="$BASE"
REG="$BASE/state/court.registry"

echo "⚖️ JUDGE JUDY SYSTEM REVIEW"
echo "=========================="

while IFS='|' read -r name state pid; do
    [ "$name" = "# NAME" ] && continue

    case "$state" in
        RUNNING)
            verdict="✔ STABLE"
            ;;
        STOPPED)
            verdict="⚠ FAILURE"
            ;;
        *)
            verdict="❓ UNKNOWN"
            ;;
    esac

    echo "$name -> $verdict (pid:$pid)"
done < "$REG"

echo ""
echo "⚖️ END OF EXTERNAL REVIEW"
