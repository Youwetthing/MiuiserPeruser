# peruse_ui.sh - card renderer for Peruse Control. Display only.
C0=$'\033[0m'; B=$'\033[1m'
GRN=$'\033[38;5;78m'; YEL=$'\033[38;5;221m'; RED=$'\033[38;5;203m'
CYN=$'\033[38;5;80m'; GRY=$'\033[38;5;245m'

bar() { local n=$1 c=$2 i o=""; for ((i=1;i<=5;i++)); do ((i<=n)) && o+="${c}▰" || o+="${GRY}▱"; done; printf '%s%s' "$o" "$C0"; }
dlabel() { case $1 in 0) echo "none known";; 1|2) echo "light";; 3) echo "phones home";; *) echo "heavy: analytics/ads";; esac; }

# card <pkg> <label> <tier> <ram_mb|-> <data 0-5> <tied to> <you lose>
card() {
  local pkg=$1 label=$2 tier=$3 ram=$4 data=$5 links=$6 lose=$7 tc head rb=0 rt="not measured"
  case $tier in
    kill)    tc=$GRN; head="🧹 BLOAT · fine to go" ;;
    caution) tc=$YEL; head="🤔 THINK TWICE" ;;
    *)       tc=$RED; head="⛔ HANDS OFF" ;;
  esac
  if [[ $ram =~ ^[0-9]+$ ]]; then rb=$(( ram<10?1 : ram<30?2 : ram<60?3 : ram<120?4 : 5 )); rt="${ram} MB now"; fi
  printf '%s╭─ %s%s%s  %s%s%s\n' "$tc" "$B" "$label" "$C0" "$GRY" "$pkg" "$C0"
  printf '%s│%s %s%s%s\n' "$tc" "$C0" "$tc" "$head" "$C0"
  printf '%s│%s 🐏 RAM     %s  %s%s%s\n' "$tc" "$C0" "$(bar $rb "$YEL")" "$GRY" "$rt" "$C0"
  printf '%s│%s 📡 DATA    %s  %s%s%s\n' "$tc" "$C0" "$(bar "$data" "$RED")" "$GRY" "$(dlabel "$data")" "$C0"
  printf '%s│%s 🔗 TIED TO %s%s%s\n' "$tc" "$C0" "$CYN" "$links" "$C0"
  printf '%s│%s 💔 YOU LOSE %s\n' "$tc" "$C0" "$lose"
  printf '%s╰──────────────────────────────%s\n\n' "$tc" "$C0"
}

if [[ "${BASH_SOURCE[0]}" == "$0" && "$1" == "--demo" ]]; then
  echo -e "\n  ✦ PERUSE · what's on your phone ✦\n"
  card com.spotify.music Spotify kill 140 3 "Spotify account" "the app"
  card com.miui.cloudservice "Mi Cloud" caution 38 4 "Mi Account · Backup · Sync" "cloud backup and sync"
  card com.android.keychain Keychain never 6 0 "Wi-Fi · VPN · certificates" "Wi-Fi and VPN logins break"
  echo "  (demo values; real cards use the TSV and live RAM)"
fi
