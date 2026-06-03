#!/usr/bin/env python3
import os, sys

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

try:
    from daemons.powerhouse import PowerhouseDaemon
except ImportError as e:
    print(f"[!] Critical Path Error: {e}")
    sys.exit(1)

def main():
    print("=" * 50)
    print("   SYNDICATE FLIP-SWITCH v2 — Dashboard Control")
    print("=" * 50)

    engine = PowerhouseDaemon()

    if not engine.check_connection():
        print("[!] ERROR: Sewer Pipe not responding.")
        print("[?] Is the Shadow Worker running via ADB?")
        sys.exit(1)

    print("\n[Flip] All daemons toggleable at dashboard level.")

    try:
        engine.start_loop()
    except KeyboardInterrupt:
        engine.stop_loop()
        print("\n[Flip] Syndicate going dark.")

if __name__ == "__main__":
    main()
