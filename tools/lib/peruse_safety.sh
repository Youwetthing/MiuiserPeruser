# peruse_safety.sh - reversible debloat, undo log, recovery card. No pm clear, ever.
UNDO_LOG="${UNDO_LOG:-data/peruse_undo.tsv}"
RECOVERY="${RECOVERY:-data/RECOVERY.txt}"

is_overlay_pkg() { case "$1" in *overlay*|*.rro.*|*_rro*|*.res|android) return 0 ;; esac; return 1; }

guard_pkg() {
  local p="${1%%:*}"
  if is_overlay_pkg "$p"; then echo "⛔ $p is an overlay/core: blocked."; return 1; fi
  if declare -F tier_guard >/dev/null && ! tier_guard "$p"; then echo "⛔ $p is protected: blocked."; return 1; fi
  return 0
}

do_action() {  # do_action <disable|uninstall|suspend> <pkg>
  local act="$1" pkg="${2%%:*}" cmd undo
  guard_pkg "$pkg" || return 1
  case "$act" in
    disable)   cmd="pm disable-user --user 0 $pkg"; undo="pm enable --user 0 $pkg" ;;
    uninstall) cmd="pm uninstall -k --user 0 $pkg"; undo="cmd package install-existing --user 0 $pkg" ;;
    suspend)   cmd="pm suspend --user 0 $pkg";      undo="pm unsuspend --user 0 $pkg" ;;
    *) echo "unknown action: $act"; return 1 ;;
  esac
  mkdir -p "$(dirname "$UNDO_LOG")"
  printf '%s\t%s\t%s\t%s\n' "$(date +%s)" "$pkg" "$act" "$undo" >> "$UNDO_LOG"; chmod 600 "$UNDO_LOG"
  if privileged "$cmd" </dev/null 2>&1 | grep -qiE 'success|new state|suspended state'; then
    echo "✅ $act $pkg (undo logged)"
  else
    echo "⚠️  $act $pkg: system gave no confirmation"; return 1
  fi
}

run_undo() {
  [[ "$1" =~ ^(pm\ enable|pm\ unsuspend|cmd\ package\ install-existing)\ --user\ 0\ [A-Za-z0-9._]+$ ]] \
    || { echo "skipped (unexpected command): $1"; return 1; }
  privileged "$1" </dev/null 2>&1 | tail -1
}

restore_one() {  # newest change first
  local pkg="$1" u
  [[ -s "$UNDO_LOG" ]] || { echo "Nothing logged yet."; return 1; }
  awk -F'\t' -v p="$pkg" '$2==p {print $4}' "$UNDO_LOG" | tac | while IFS= read -r u; do run_undo "$u"; done
}

restore_all() {
  [[ -s "$UNDO_LOG" ]] || { echo "Nothing logged yet."; return 0; }
  local pkg
  for pkg in $(awk -F'\t' '{print $2}' "$UNDO_LOG" | sort -u); do echo "↩️  $pkg"; restore_one "$pkg"; done
}

history_list() {
  [[ -s "$UNDO_LOG" ]] || { echo "No changes logged yet."; return 0; }
  local e p a _
  while IFS=$'\t' read -r e p a _; do printf '%s  %-9s %s\n' "$(date -d "@$e" '+%F %R')" "$a" "$p"; done < "$UNDO_LOG"
}

write_recovery() {
  mkdir -p "$(dirname "$RECOVERY")"
  {
    echo "PERUSE RECOVERY CARD ($(date '+%F %R'))"
    echo
    echo "Apps missing or phone stuck? From a computer with adb and USB debugging, run:"
    echo
    if [[ -s "$UNDO_LOG" ]]; then awk -F'\t' '{print $4}' "$UNDO_LOG" | sort -u | tac | sed 's/^/  adb shell /'; else echo "  (nothing logged yet)"; fi
    echo
    echo "If it won't boot (key combos vary by model):"
    echo "  1. Power + Volume Up to reach Recovery."
    echo "  2. Try safe mode: long-press Power off on the power menu."
    echo "  3. Last resort: Recovery > Wipe data (erases everything)."
  } > "$RECOVERY"; chmod 600 "$RECOVERY"
  if [[ -d "$HOME/storage/shared" ]]; then cp "$RECOVERY" "$HOME/storage/shared/peruse_recovery.txt" && echo "Copied to phone storage: peruse_recovery.txt"
  else echo "Tip: run termux-setup-storage once to also save it to phone storage."; fi
  echo "Saved: $RECOVERY"
}

debloat_review() {  # walk kill-tier apps that are installed and enabled
  local tf="${TIER_FILE:-data/peruse_apps.tsv}" enabled p tier status note ans n=0
  enabled=$(privileged "pm list packages -e" </dev/null | tr -d '\r' | sed 's/package://')
  while IFS=$'\t' read -r -u 3 p tier status note; do
    [[ "$tier" == "kill" ]] || continue
    grep -qxF "$p" <<<"$enabled" || continue
    n=$((n+1)); echo; echo "🧹 $p"; echo "   ${note:-No description available} [$status]"
    read -r -n 1 -p "   Remove for this user? [y/N] " ans </dev/tty; echo
    [[ "$ans" == [yY] ]] && do_action uninstall "$p"
  done 3< "$tf"
  [[ $n -eq 0 ]] && echo "Nothing left on the kill list. 🎉"
  [[ -s "$UNDO_LOG" ]] && write_recovery >/dev/null && echo "Recovery card refreshed."
}

safety_menu() {
  local c p
  while true; do
    clear; echo "🛟 DEBLOAT SAFETY NET"; echo
    echo "1) 🧹 Review the kill list (y/N each, undoable)"
    echo "2) 📜 Undo history"
    echo "3) ↩️  Restore one app"
    echo "4) ↩️  Restore everything"
    echo "5) 🗒️  Write recovery card"
    echo "b) Back"; echo
    read -r -p "Choice: " c
    case $c in
      1) debloat_review; read -r -n 1 -s -p "Press any key..." ;;
      2) history_list;   read -r -n 1 -s -p "Press any key..." ;;
      3) read -r -p "Package: " p; restore_one "$p"; sleep 2 ;;
      4) restore_all;    read -r -n 1 -s -p "Press any key..." ;;
      5) write_recovery; read -r -n 1 -s -p "Press any key..." ;;
      b|B) return ;;
    esac
  done
}
