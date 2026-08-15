#!/usr/bin/env python3
"""
MiuiserPeruser ShellCheck Fix Script v2
Handles edge cases that v1 missed.
"""
import re, os

def fix_multi_sensory_room(path):
    """SC2183 in multi_sensory_room.sh — uses echo -e wrapper"""
    with open(path) as f:
        content = f.read()
    
    orig = content
    # Pattern: $(printf '%*s' "VAR" | tr ' ' 'CHAR')
    content = re.sub(
        r"printf '%\*s'\s+\"([^\"]+)\"\s*\|\s*tr\s+'\s+'\s+'([^']+)'",
        r"printf '%*s' \"\1\" '' | tr ' ' '\2'",
        content
    )
    # Also handle: $(printf '%*s' "$(( W - 2 ))" | tr ' ' '-')
    content = re.sub(
        r"printf '%\*s'\s+(\"\$\(\([^)]+\)\)\")\s*\|\s*tr\s+'\s+'\s+'([^']+)'",
        r"printf '%*s' \1 '' | tr ' ' '\2'",
        content
    )
    # Also handle unquoted: printf '%*s' $inner | tr ' ' '-'
    content = re.sub(
        r"printf '%\*s'\s+(\$[a-z_]+)\s*\|\s*tr\s+'\s+'\s+'([^']+)'",
        r"printf '%*s' \1 '' | tr ' ' '\2'",
        content
    )
    
    if content != orig:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  ✓ Fixed SC2183: {path}")
    else:
        print(f"  ○ No changes: {path}")
        # Debug: show what we're looking for
        matches = re.findall(r"printf '%\*s'[^)]*\)", orig)
        if matches:
            print(f"     Found patterns: {matches[:3]}")

def fix_stalkerslayer_trap(path):
    """SC2064 — trap with double quotes expanding $LOCK_DIR"""
    with open(path) as f:
        content = f.read()
    
    orig = content
    content = re.sub(
        r'trap\s+"rmdir\s+\'?(\$[A-Z_]+)\'?\s+2>/dev/null;\s*exit"\s+EXIT',
        r"trap 'rmdir \1 2>/dev/null; exit' EXIT",
        content
    )
    
    if content != orig:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  ✓ Fixed SC2064: {path}")
    else:
        print(f"  ○ No changes: {path}")
        for i, line in enumerate(content.split('\n'), 1):
            if 'trap' in line and 'LOCK_DIR' in line:
                print(f"     Line {i}: {line.strip()}")

def fix_battery_engine(path):
    """SC2221/2222 — overlapping case patterns"""
    with open(path) as f:
        content = f.read()
    
    orig = content
    content = content.replace('mt67*|mt6768|mt6785)', 'mt6768|mt6785|mt67*)')
    content = content.replace('mt67* | mt6768 | mt6785)', 'mt6768 | mt6785 | mt67*)')
    
    if content != orig:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  ✓ Fixed SC2221/2222: {path}")
    else:
        print(f"  ○ No changes: {path}")
        for i, line in enumerate(content.split('\n'), 1):
            if 'mt67' in line and ')' in line:
                print(f"     Line {i}: {line.strip()}")

def fix_kernel_sanders(path):
    """SC2221/2222 — *mi*|*xiaomi* pattern"""
    with open(path) as f:
        content = f.read()
    
    orig = content
    content = content.replace('*mi*|*xiaomi*)', '*mi*)')
    content = content.replace('*mi* | *xiaomi*)', '*mi*)')
    
    if content != orig:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  ✓ Fixed SC2221/2222: {path}")
    else:
        print(f"  ○ No changes: {path}")
        for i, line in enumerate(content.split('\n'), 1):
            if '*mi*' in line and ')' in line:
                print(f"     Line {i}: {line.strip()}")

def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    print("═" * 60)
    print("MiuiserPeruser ShellCheck Fix Script v2")
    print("═" * 60)
    
    print("\n📐 Fixing SC2183 in multi_sensory_room.sh...")
    if os.path.exists('multi_sensory_room.sh'):
        fix_multi_sensory_room('multi_sensory_room.sh')
    
    print("\n📐 Fixing SC2064 in StalkerSlayer.sh...")
    if os.path.exists('StalkerSlayer.sh'):
        fix_stalkerslayer_trap('StalkerSlayer.sh')
    
    print("\n📐 Fixing SC2221/2222 in battery_engine.sh...")
    if os.path.exists('battery_engine.sh'):
        fix_battery_engine('battery_engine.sh')
    
    print("\n📐 Fixing SC2221/2222 in kernel_sanders.sh...")
    if os.path.exists('kernel_sanders.sh'):
        fix_kernel_sanders('kernel_sanders.sh')
    
    print("\n" + "═" * 60)
    print("Done.")
    print("═" * 60)

if __name__ == '__main__':
    main()
