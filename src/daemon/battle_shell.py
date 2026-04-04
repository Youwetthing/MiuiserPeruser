import subprocess
import time
import os
import re

PIPE_PATH = "/data/local/tmp/.hitlist"

# KILL ON SIGHT (Low risk, high reward)
KOS_LIST = ["com.miui.miwallpaper", "com.miui.msa.global", "com.miui.analytics"]

# THE PROTECTED RATS (Must be lobotomized, not killed)
STARVE_LIST = ["com.miui.daemon", "com.miui.securitycenter", "com.xiaomi.joyose"]

print("🚐 BATTLE SHELL 2.1: Elevenerife Intelligence Active.")

def get_actual_pkg(pkg_fragment):
    try:
        # Find the real package name from the process list
        out = subprocess.check_output(["rish", "-c", f"pm list packages | grep {pkg_fragment}"], stderr=subprocess.STDOUT).decode()
        match = re.search(r"package:(.*)", out)
        return match.group(1).strip() if match else None
    except:
        return None

def lobotomize(pkg):
    print(f"💉 LOBOTOMIZING: {pkg}...")
    # 1. Strip background data via netpolicy
    subprocess.run(f'rish -c "cmd netpolicy set restrict-background true"', shell=True, capture_output=True)
    # 2. Use appops to ignore background execution (The Puncture)
    subprocess.run(f'rish -c "cmd appops set {pkg} RUN_IN_BACKGROUND ignore"', shell=True, capture_output=True)
    # 3. Disable its ability to start other services
    subprocess.run(f'rish -c "cmd appops set {pkg} START_FOREGROUND ignore"', shell=True, capture_output=True)
    # 4. Force into the Restricted bucket (Silent Sleep)
    subprocess.run(f'rish -c "am set-standby-bucket {pkg} restricted"', shell=True, capture_output=True)

def drop_bounty(pkg):
    print(f"🎯 BOUNTY: {pkg} -> Granitor")
    subprocess.run(f'rish -c "echo {pkg} > {PIPE_PATH}"', shell=True, capture_output=True)

while True:
    try:
        ps_out = subprocess.check_output(["rish", "-c", "ps -ef"]).decode('utf-8')
        
        # Phase 1: Pure Execution
        for rat in KOS_LIST:
            if rat in ps_out:
                drop_bounty(rat)
        
        # Phase 2: Tactical Lobotomy
        for rat in STARVE_LIST:
            real_name = get_actual_pkg(rat)
            if real_name and real_name in ps_out:
                lobotomize(real_name)
        
        # Keep the Bridge alive
        subprocess.run(["rish", "-c", "settings put global adb_enabled 1"], capture_output=True)
        
    except Exception as e:
        if "timeout" in str(e).lower():
            print("🧊 Shizuku Frozen. Tactical retreat (15s)...")
            time.sleep(15)
            
    time.sleep(7) # Relaxed interval to prevent Shizuku 'Mask' fatigue
