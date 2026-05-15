#!/usr/bin/env python3
import os, sys, time

# Ensure the project root is on sys.path so 'daemons' package is importable
# regardless of the working directory at launch time.
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

from daemons.powerhouse import PowerhouseDaemon

def main():
    print("="*50)
    print("   SYNDICATE FLIP-SWITCH v2 — Dashboard Control")
    print("="*50)

    engine = PowerhouseDaemon()

    print("\n[Flip] All daemons are now toggleable at dashboard level.")
    print("[Flip] Ghost cloaking, Granitor brains, etc. controlled via DB flag.")

    try:
        engine.start_loop()
    except KeyboardInterrupt:
        engine.stop_loop()
        print("\n[Flip] Syndicate going dark.")

if __name__ == "__main__":
    main()
