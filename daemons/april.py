"""
April Table — shared memory layout for MiuiserPeruser runtime flags.

Offset  Size  Flag
0–3     4B    ROUTE_MODE    0=AUTO  1=UNIX_ONLY  2=TCP_ONLY
4–7     4B    KRANG_MODE    0=ACTIVE  1=PASS_THROUGH  2=OFFLINE
8–11    4B    LOG_LEVEL     0=MINIMAL  1=NORMAL  2=VERBOSE
12–15   4B    LOAD_MONITOR  0=OFF  1=ON  2=THROTTLED
16–19   4B    TCP_FALLBACK  0=DISABLED  1=ENABLED
20–23   4B    SYSTEM_LOCK   0=NORMAL  1=LOCKED
24–27   4B    POLL_THROTTLE 0=NORMAL  1=THROTTLED(3x)  2=MINIMAL(10x)

All values uint32 big-endian. flip_switch.py is sole writer.
"""
import struct

APRIL_SIZE = 4096
FLAG_FMT   = "!I"

OFFSETS = {
    "ROUTE_MODE":    0,
    "KRANG_MODE":    4,
    "LOG_LEVEL":     8,
    "LOAD_MONITOR":  12,
    "TCP_FALLBACK":  16,
    "SYSTEM_LOCK":   20,
    "POLL_THROTTLE": 24,
}

DEFAULTS = {
    "ROUTE_MODE":    0,
    "KRANG_MODE":    0,
    "LOG_LEVEL":     1,
    "LOAD_MONITOR":  1,
    "TCP_FALLBACK":  1,
    "SYSTEM_LOCK":   0,
    "POLL_THROTTLE": 0,
}

def read_flag(mm, flag):
    mm.seek(OFFSETS[flag])
    return struct.unpack(FLAG_FMT, mm.read(4))[0]

def write_flag(mm, flag, value):
    mm.seek(OFFSETS[flag])
    mm.write(struct.pack(FLAG_FMT, value))
    mm.flush()

def read_all(mm):
    return {flag: read_flag(mm, flag) for flag in OFFSETS}

def write_defaults(mm):
    for flag, value in DEFAULTS.items():
        write_flag(mm, flag, value)
    mm.flush()
