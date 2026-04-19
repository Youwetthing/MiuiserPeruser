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

while true; do

    if read line < "$IN_PIPE"; then

        # SOURCE|SIGNAL|SCORE|CONTEXT
        IFS="|" read -r src sig score ctx <<< "$line"

        id=$(case_id)

        log "Case received: $id ($src)"

        write_case "$id" "$src" "$sig" "$score" "$ctx"

        forward_to_judgement "$src" "$sig" "$score" "$ctx"

    fi

done
