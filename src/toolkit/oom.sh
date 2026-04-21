#!/data/data/com.termux/files/usr/bin/bash
# Something Kinda OOM — Bash TUI powered by C scanner
# Temp file stored inside toolkit folder (permanent fixture)

PAGE_SIZE=20
page=0

toolkit_dir="$HOME/MiuiserPeruser/src/toolkit"
scanner="$toolkit_dir/something_kinda_oom"
temp_data="$toolkit_dir/oom_data.$$"
uid_map_file="$HOME/MiuiserPeruser/data/uid_package_map.txt"

# Ensure toolkit dir exists (it does, but belt + braces)
mkdir -p "$toolkit_dir"

# Load UID → package map
declare -A uid_name
if [[ -r "$uid_map_file" ]]; then
    while IFS= read -r line; do
        [[ -z "$line" || "$line" =~ ^# ]] && continue
        uid="${line%% *}"
        pkg="${line#* }"
        uid_name["$uid"]="$pkg"
    done < "$uid_map_file"
fi

oom_to_bucket() {
    local oom=$1
    (( oom == 0 )) && echo "SYSTEM" && return
    (( oom <= 100 )) && echo "FOREGROUND" && return
    (( oom <= 200 )) && echo "VISIBLE" && return
    (( oom <= 300 )) && echo "PERCEPTIBLE" && return
    (( oom <= 400 )) && echo "BACKUP" && return
    (( oom <= 500 )) && echo "HEAVY_WEIGHT" && return
    (( oom <= 600 )) && echo "SERVICE" && return
    (( oom <= 700 )) && echo "HOME" && return
    (( oom <= 800 )) && echo "PREVIOUS" && return
    (( oom <= 900 )) && echo "SERVICE_B" && return
    echo "CACHED"
}

# Bulletproof fetch
fetch_data() {
    if [[ -x "$scanner" ]]; then
        "$scanner" > "$temp_data" 2>/dev/null
    fi
    [[ -f "$temp_data" ]] || : > "$temp_data"
}

# Initial fetch
fetch_data

draw() {
    printf "\033[H\033[J"
    echo -e "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"
    echo -e "\033[1;33mSomething Kinda OOM — Android Protection Rooms\033[0m"
    echo -e "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"
    printf "\033[1;37m%6s %4s %-12s %-16s %s\033[0m\n" "PID" "OOM" "BUCKET" "PACKAGE" "CMD"
    echo -e "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"

    mapfile -t all_lines < "$temp_data"
    local total=${#all_lines[@]}
    local start=$((page * PAGE_SIZE))
    local end=$((start + PAGE_SIZE))

    for ((i=start; i<end && i<total; i++)); do
        line="${all_lines[i]}"
        set -- $line
        pid=$1; oom=$2; uid=$3; shift 3; cmd="$*"

        bucket=$(oom_to_bucket "$oom")
        pkg="${uid_name[$uid]:-uid:$uid}"

        if (( oom >= 500 )); then color="\033[91m"
        elif (( oom >= 200 )); then color="\033[93m"
        else color="\033[92m"; fi

        printf "${color}%6d %4d %-12s %-16s %s\033[0m\n" \
               "$pid" "$oom" "$bucket" "${pkg::16}" "${cmd::50}"
    done

    local total_pages=$(((total + PAGE_SIZE - 1) / PAGE_SIZE))
    (( total_pages == 0 )) && total_pages=1

    echo -e "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m"
    printf "\033[2mPage %d/%d (showing %d-%d of %d) | Time: %(%H:%M:%S)T\033[0m\n" \
           $((page+1)) "$total_pages" \
           $((total==0?0:start+1)) $((end>total?total:end)) "$total" -1
    echo "Commands: [n]ext [p]rev [r]efresh [k]ill [q]uit"
}

while true; do
    draw
    read -rsn1 key
    case "$key" in
        n)
            if (( (page+1)*PAGE_SIZE < $(wc -l < "$temp_data") )); then
                ((page++))
            fi
            ;;
        p)
            (( page > 0 )) && ((page--))
            ;;
        r)
            fetch_data
            page=0
            ;;
        k)
            echo -n "PID to kill: "
            read pid_input
            uid=""
            while IFS= read -r line; do
                set -- $line
                if [[ $1 == "$pid_input" ]]; then
                    uid=$3
                    break
                fi
            done < "$temp_data"

            if [[ -z "$uid" ]]; then
                echo "PID not found"
            elif (( uid < 10000 )); then
                echo "Refusing to kill system process (UID $uid)"
            else
                rish -c "kill -9 $pid_input" >/dev/null 2>&1 \
                    && echo "Killed $pid_input" \
                    || echo "Kill failed"
            fi

            echo "Press Enter to continue..."
            read dummy
            ;;
        q)
            rm -f "$temp_data"
            break
            ;;
    esac
done
