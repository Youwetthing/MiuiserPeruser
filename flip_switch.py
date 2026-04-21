#!/usr/bin/env python3
import sys, os, mmap
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from daemons.april import (APRIL_SIZE, OFFSETS, DEFAULTS,
                            read_flag, write_flag, read_all, write_defaults)

BASE_DIR  = "/data/data/com.termux/files/home/MiuiserPeruser"
APRIL_BIN = BASE_DIR + "/Database/april.bin"

VALUE_LABELS = {
    "ROUTE_MODE":    {0:"AUTO", 1:"UNIX_ONLY", 2:"TCP_ONLY"},
    "KRANG_MODE":    {0:"ACTIVE", 1:"PASS_THROUGH", 2:"OFFLINE"},
    "LOG_LEVEL":     {0:"MINIMAL", 1:"NORMAL", 2:"VERBOSE"},
    "LOAD_MONITOR":  {0:"OFF", 1:"ON", 2:"THROTTLED"},
    "TCP_FALLBACK":  {0:"DISABLED", 1:"ENABLED"},
    "SYSTEM_LOCK":   {0:"NORMAL", 1:"LOCKED"},
    "POLL_THROTTLE": {0:"NORMAL", 1:"THROTTLED", 2:"MINIMAL"},
}

VALUE_KEYS = {f: {v:k for k,v in l.items()} for f,l in VALUE_LABELS.items()}

SAFETY_RULES = [
    ("TCP_FALLBACK", 0, "ROUTE_MODE", 2,
     "ROUTE_MODE=TCP_ONLY requires TCP_FALLBACK=ENABLED."),
    ("KRANG_MODE", 2, "ROUTE_MODE", 1,
     "ROUTE_MODE=UNIX_ONLY with KRANG_MODE=OFFLINE deadlocks the stack."),
    ("KRANG_MODE", 2, "TCP_FALLBACK", 0,
     "KRANG_MODE=OFFLINE with TCP_FALLBACK=DISABLED leaves no execution path."),
]

def open_april():
    if not os.path.exists(APRIL_BIN):
        print("[FlipSwitch] FATAL: april.bin missing. Run install.sh first.")
        sys.exit(1)
    f = open(APRIL_BIN, "r+b")
    mm = mmap.mmap(f.fileno(), APRIL_SIZE)
    return f, mm

def resolve_value(flag, value_str):
    if value_str.isdigit(): return int(value_str)
    key = value_str.upper()
    if key not in VALUE_KEYS[flag]:
        print(f"[FlipSwitch] Unknown value '{value_str}' for {flag}. Valid: {', '.join(VALUE_KEYS[flag])}")
        sys.exit(1)
    return VALUE_KEYS[flag][key]

def check_safety(current_state, target_flag, target_value):
    proposed = dict(current_state)
    proposed[target_flag] = target_value
    for (cf, cv, bf, bv, reason) in SAFETY_RULES:
        if proposed.get(cf) == cv and proposed.get(bf) == bv:
            return False, reason
    return True, None

def cmd_status(mm):
    print("\n[FlipSwitch] Current runtime flags:\n")
    for flag, raw in read_all(mm).items():
        label = VALUE_LABELS[flag].get(raw, f"UNKNOWN({raw})")
        print(f"  {flag:<16} {raw}  ({label})")
    print()

def cmd_set(mm, flag, value_str):
    flag = flag.upper()
    if flag not in OFFSETS:
        print(f"[FlipSwitch] Unknown flag: {flag}")
        sys.exit(1)
    value = resolve_value(flag, value_str)
    current = read_all(mm)
    safe, reason = check_safety(current, flag, value)
    if not safe:
        print(f"[FlipSwitch] BLOCKED: {reason}")
        sys.exit(1)
    old = VALUE_LABELS[flag].get(current[flag], str(current[flag]))
    new = VALUE_LABELS[flag].get(value, str(value))
    write_flag(mm, flag, value)
    print(f"[FlipSwitch] {flag}: {old} → {new}")

def cmd_reset(mm):
    write_defaults(mm)
    print("[FlipSwitch] All flags reset to defaults.")
    cmd_status(mm)

def usage():
    print("Usage: flip_switch.py status | set <FLAG> <VALUE> | reset")
    for flag, labels in VALUE_LABELS.items():
        vals = "  ".join(f"{v}={k}" for v,k in labels.items())
        print(f"  {flag:<16} {vals}")

if __name__ == "__main__":
    if len(sys.argv) < 2: usage(); sys.exit(1)
    f, mm = open_april()
    try:
        cmd = sys.argv[1].lower()
        if cmd == "status": cmd_status(mm)
        elif cmd == "set":
            if len(sys.argv) < 4: usage(); sys.exit(1)
            cmd_set(mm, sys.argv[2], sys.argv[3])
        elif cmd == "reset": cmd_reset(mm)
        else: usage(); sys.exit(1)
    finally:
        mm.close(); f.close()
