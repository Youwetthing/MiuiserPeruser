#!/usr/bin/env python3
import re, os, glob

def fix_sc2183(content):
    """Fix printf '%*s' missing second argument, and printf '%Ns' missing arg."""
    # Pattern: printf '%*s' WIDTH | tr → needs WIDTH '' | tr
    # Handles: printf '%*s' "$COLS" | tr ' ' '-'  →  printf '%*s' "$COLS" '' | tr ' ' '-'
    # Handles: printf '%*s' 76 | tr ' ' "${BOX_H}"  →  printf '%*s' 76 '' | tr ' ' "${BOX_H}"
    # Handles: printf '%80s' | tr ' ' '-'  →  printf '%80s' '' | tr ' ' '-'
    
    # Fix %*s with quoted width
    content = re.sub(
        r"printf '%\*s'\s+([\"\'][^\"\']+[\"\'])\s*\|\s*tr\s+([\"\'][^\"\']+[\"\'])\s+([\"\'][^\"\']+[\"\'])",
        r"printf '%*s' \1 '' | tr \2 \3",
        content
    )
    # Fix %*s with numeric width
    content = re.sub(
        r"printf '%\*s'\s+(\d+)\s*\|\s*tr\s+([\"\'][^\"\']+[\"\'])\s+([\"\'][^\"\']+[\"\'])",
        r"printf '%*s' \1 '' | tr \2 \3",
        content
    )
    # Fix %Ns (e.g., %80s) with no args at all
    content = re.sub(
        r"printf '(%\d+s)'\s*\|\s*tr\s+([\"\'][^\"\']+[\"\'])\s+([\"\'][^\"\']+[\"\'])",
        r"printf '\1' '' | tr \2 \3",
        content
    )
    return content

def fix_sc2188(content):
    """Fix bare redirection: > "$VAR" → : > "$VAR" """
    return re.sub(r'^\s*>\s*("[^"]+")', r': > \1', content, flags=re.MULTILINE)

def fix_sc2155(content):
    """Fix local/readonly var=$(cmd) → split declaration and assignment."""
    lines = content.split('\n')
    new_lines = []
    for line in lines:
        # Match: local var=$(...) or readonly var=$(...) or local var="string"
        m = re.match(r'^(\s*)(local|readonly)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(.*)$', line)
        if m and ('$(' in m.group(4) or m.group(4).startswith('"')):
            indent, decl, var, rest = m.groups()
            new_lines.append(f"{indent}{decl} {var}")
            new_lines.append(f"{indent}{var}={rest}")
        else:
            new_lines.append(line)
    return '\n'.join(new_lines)

def fix_sc2064(content):
    """Fix trap double-quote expansion."""
    return re.sub(
        r'trap\s+"rmdir \'(\$[A-Z_]+)\' 2>/dev/null; exit"\s+EXIT',
        r"trap 'rmdir \1 2>/dev/null; exit' EXIT",
        content
    )

def fix_sc2221_battery(content):
    """Fix overlapping case patterns in battery_engine.sh"""
    # mt67*|mt6768|mt6785 → mt6768|mt6785|mt67*
    content = content.replace('mt67*|mt6768|mt6785)', 'mt6768|mt6785|mt67*)')
    return content

def fix_sc2221_kernel(content):
    """Fix overlapping case patterns in kernel_sanders.sh"""
    # *mi*|*xiaomi*) → *mi*)
    content = content.replace('*mi*|*xiaomi*)', '*mi*)')
    return content

def fix_sc2309_peruse(content):
    """Fix peruse_control.sh variable naming bug."""
    # "$total"_installed → "$total_installed" (if that's the intended var)
    # Actually looking at the code, it seems like $total_installed is the var name
    # But the quotes are broken: ""$total"_installed" should be "$total_installed"
    content = re.sub(r'""\$total""_installed', '"$total_installed"', content)
    content = re.sub(r'"\$total"_installed', '"$total_installed"', content)
    content = re.sub(r'""\$total""', '"$total"', content)
    content = re.sub(r'of\s+"\$total"\._installed', 'of "$total_installed"', content)
    return content

def process_file(path, fixes):
    with open(path, 'r') as f:
        original = f.read()
    
    content = original
    for fix_name, fix_func in fixes:
        content = fix_func(content)
    
    if content != original:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  ✓ Fixed: {os.path.basename(path)}")
        return True
    else:
        print(f"  ○ No changes: {os.path.basename(path)}")
        return False

def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    print("═" * 60)
    print("MiuiserPeruser ShellCheck Fix Script")
    print("═" * 60)
    
    # Fix SC2183 in all files
    sc2183_files = [
        'multi_sensory_room.sh', 'april_oneil.sh', 'bell_tower.sh',
        'foresight.sh', 'discover.sh', 'app_netwatch.sh'
    ]
    print("\n📐 Fixing SC2183 (printf '%*s' missing arg)...")
    for f in sc2183_files:
        if os.path.exists(f):
            process_file(f, [('sc2183', fix_sc2183)])
    
    # Fix SC2188
    print("\n📐 Fixing SC2188 (bare redirection)...")
    for f in ['ram_slammer_v2.sh', 'ram_slammer_v2.2.sh']:
        if os.path.exists(f):
            process_file(f, [('sc2188', fix_sc2188)])
    
    # Fix SC2155 in specific files
    print("\n📐 Fixing SC2155 (local/readonly split)...")
    for f in ['StalkerSlayer.sh', 'april_oneil.sh', 'dialer_spy.sh']:
        if os.path.exists(f):
            process_file(f, [('sc2155', fix_sc2155)])
    
    # Fix SC2064
    print("\n📐 Fixing SC2064 (trap quote expansion)...")
    if os.path.exists('StalkerSlayer.sh'):
        process_file('StalkerSlayer.sh', [('sc2064', fix_sc2064)])
    
    # Fix SC2221/2222
    print("\n📐 Fixing SC2221/2222 (overlapping case patterns)...")
    if os.path.exists('battery_engine.sh'):
        process_file('battery_engine.sh', [('sc2221', fix_sc2221_battery)])
    if os.path.exists('kernel_sanders.sh'):
        process_file('kernel_sanders.sh', [('sc2221', fix_sc2221_kernel)])
    
    # Fix peruse_control SC2309/SC2027
    print("\n📐 Fixing SC2309/SC2027 (peruse_control quoting)...")
    if os.path.exists('peruse_control.sh'):
        process_file('peruse_control.sh', [('sc2309', fix_sc2309_peruse)])
    
    print("\n" + "═" * 60)
    print("Done. Run: shellcheck -s bash *.sh")
    print("═" * 60)

if __name__ == '__main__':
    main()
