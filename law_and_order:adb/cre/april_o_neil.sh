#!/data/data/com.termux/files/usr/bin/bash

# =========================
# APRIL_O_NEIL (CRE DAEMON)
# Case Assembly Engine
# =========================

BASE="$HOME/MiuiserPeruser"
IN_PIPE="$BASE/pipes/superhero.pipe"
OUT_PIPE="$BASE/pipes/judgement.pipe"
CASE_DIR="$BASE/cre/cases"
LOG="$BASE/cre/april.log"

mkdir -p "$CASE_DIR"
MAX_CASES=500
mkdir -p "$BASE/cre"

log() {
    echo "[APRIL_CRE] $1" >> "$LOG"
}

case_id() {
    date +%s%N | cut -b1-13
}

write_case() {
    local id="$1"
    local src="$2"
    local sig="$3"
    local score="$4"
    local ctx="$5"

    cat <<JSON > "$CASE_DIR/case_$id.json"
{
  "case_id": "$id",
  "source": "$src",
  "signal": "$sig",
  "score": "$score",
  "context": "$ctx",
  "status": "PENDING_JUDGEMENT"
}
JSON
}

forward_to_judgement() {
    local src="$1"
    local sig="$2"
    local score="$3"
    local ctx="$4"

    echo "$src|$sig|$score|$ctx" >> "$OUT_PIPE"
}

log "CRE ONLINE - APRIL_O_NEIL LOCKED"

# Ensure pipes exist
[ -p "$IN_PIPE" ] || mkfifo "$IN_PIPE"
[ -p "$OUT_PIPE" ] || mkfifo "$OUT_PIPE"


exec 3<>"$IN_PIPE"
while IFS="|" read -r src sig score ctx <&3; do
    [ -z "$src" ] && continue

    id=$(case_id)

    log "Case received: $id ($src)"

    # enforce cap — evict oldest if at limit
    case_count=$(ls "$CASE_DIR"/case_*.json 2>/dev/null | wc -l)
    if [ "$case_count" -ge "$MAX_CASES" ]; then
        ls -t "$CASE_DIR"/case_*.json | tail -n +$((MAX_CASES)) | xargs rm -f
        log "Cap enforced: evicted $((case_count - MAX_CASES + 1)) old case(s)"
    fi

    write_case "$id" "$src" "$sig" "$score" "$ctx"

    forward_to_judgement "$src" "$sig" "$score" "$ctx"

done
