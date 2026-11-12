# peruse_catalog.sh - preinstalled apps grouped by developer. Read-only.
dev_of() {
  case "$1" in
    *overlay*|*.rro.*|*_rro*|*.res) echo "Overlays (RRO)" ;;
    com.google.*|com.android.chrome|com.android.vending) echo "Google" ;;
    com.miui.*|com.xiaomi.*|com.mi.*|com.milink.*|com.mfashiongallery.*) echo "Xiaomi" ;;
    com.unisoc*|com.sprd.*|com.spreadtrum.*|android.unisoc.*) echo "Unisoc" ;;
    com.longcheertel.*|com.silead.*|com.bsp.*) echo "Factory/ODM" ;;
    android|com.android.*) echo "Android (AOSP)" ;;
    *) echo "Other preloads" ;;
  esac
}

catalog_load() {
  declare -gA TIER; local p t _
  while IFS=$'\t' read -r p t _; do [[ -n "$p" && "$p" != \#* ]] && TIER["$p"]=$t; done < "${TIER_FILE:-data/peruse_apps.tsv}"
  SYS=(); while IFS= read -r p; do p=${p#package:}; p=${p%$'\r'}; [[ -n "$p" ]] && SYS+=("$p"); done < <(privileged "pm list packages -s")
}

catalog_summary() {
  declare -A n; local p d t
  for p in "${SYS[@]}"; do d=$(dev_of "$p"); t=${TIER[$p]:-unrated}; ((n["$d|$t"]++)); ((n["$d|all"]++)); done
  printf '%-16s %5s %6s %8s %6s %8s\n' Developer apps kill caution never unrated
  for d in Google Xiaomi "Android (AOSP)" Unisoc "Factory/ODM" "Overlays (RRO)" "Other preloads"; do
    printf '%-16s %5d %6d %8d %6d %8d\n' "$d" "${n[$d|all]:-0}" "${n[$d|kill]:-0}" "${n[$d|caution]:-0}" "${n[$d|never]:-0}" "${n[$d|unrated]:-0}"
  done
}

catalog_list() {  # catalog_list <developer>
  local p
  for p in "${SYS[@]}"; do [[ "$(dev_of "$p")" == "$1" ]] && echo "${TIER[$p]:-unrated} $p"; done | sort |
    awk '{i=($1=="kill")?"🧹":($1=="caution")?"🤔":($1=="never")?"⛔":"❔"; printf "  %s %s\n", i, $2}'
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  cd "$(dirname "$0")/../.." || exit 1
  source tools/lib/tool_backend.sh
  catalog_load; catalog_summary
  [[ -n "$1" ]] && { echo; catalog_list "$1"; }
fi
