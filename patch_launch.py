#!/usr/bin/env python3
"""Patch syndicate_scan.sh to use rish for NDK daemon launch."""
import re, shutil

TARGET = "scripts/syndicate_scan.sh"
BAK = TARGET + ".bak"

# Read original
with open(TARGET) as f:
    content = f.read()

# Backup
shutil.copy(TARGET, BAK)
print(f"Backed up to {BAK}")

# 1. Add RISH variable after BASE definition (if missing)
if 'RISH="$HOME/rish"' not in content:
    content = content.replace(
        'BASE="$HOME/MiuiserPeruser"',
        'BASE="$HOME/MiuiserPeruser"\nRISH="$HOME/rish"'
    )
    print("Added RISH variable.")

# 2. Replace _launch_daemon function
new_func = '''_launch_daemon() {
    local d="$1" bin="$2" to="$3"
    local extra=$(_daemon_env "$d")
    local result_file="$RESULTS/${d}.json"
    local log_file="$LOGS/${d}.log"
    rm -f "$result_file"
    (
        # Build command for the Android shell (no internal redirects)
        local cmd="$bin"
        [ -n "$extra" ] && cmd="export $extra; $cmd"
        cmd="timeout $to $cmd"
        if [ -x "$RISH" ]; then
            $RISH -c "$cmd"
        elif command -v adb >/dev/null 2>&1; then
            adb shell "$cmd"
        else
            exec timeout "$to" "$bin"
        fi
    ) > "$result_file" 2>> "$log_file" &
    echo $!
}'''

# Match existing function from "_launch_daemon()" to the next "}" at start of line
pattern = r'(?<=_launch_daemon\(\) \{).*?^\}'
content = re.sub(pattern, new_func.split('{',1)[1].rsplit('}',1)[0], content, flags=re.MULTILINE|re.DOTALL)

with open(TARGET, 'w') as f:
    f.write(content)

print("_launch_daemon patched successfully.")
