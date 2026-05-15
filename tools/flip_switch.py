#!/usr/bin/env python3
import os, sys, time
from daemons.powerhouse import PowerhouseDaemon

import os
import sys

# --- THE SYNDICATE PATHFINDER ---
# This ensures Python can see the 'core' and 'daemons' folders
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
if BASE_DIR not in sys.path:
    sys.path.insert(0, BASE_DIR)

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
# Now the imports are safe
try:
    from daemons.powerhouse import PowerhouseDaemon
except ImportError as e:
    print(f"[!] Critical Path Error: {e}")
    sys.exit(1)

def main():
    engine = PowerhouseDaemon()

    print("\n" + "="*30)
    print("   SYNDICATE FLIP-SWITCH   ")
    print("="*30)
    
    # Check if the Sewer Pipe (Shadow Worker) is alive
    if not engine.check_connection():
        print("[!] ERROR: Sewer Pipe not responding.")
        print("[?] Is the Shadow Worker running via ADB?")
        sys.exit(1)

    try:
        # Ignite the decision engine
        engine.start_loop()
    except KeyboardInterrupt:
        engine.stop_loop()
        print("\n[!] User Interrupted. Syndicate going dark.")

if __name__ == "__main__":
    main()
