import subprocess
import time

TARGETS_FILE = "targets.txt"

def update_targets(pkg):
    with open(TARGETS_FILE, "r") as f:
        targets = f.read().splitlines()
    if pkg not in targets:
        with open(TARGETS_FILE, "a") as f:
            f.write(f"{pkg}\n")
        print(f"[*] Brain Learned: New target {pkg} added to memory.")

def monitor():
    print("[!] Brain is reading the logs. Listening for resurrection events...")
    # This filter captures the exact moment an app is "Scheduled" to start
    cmd = ["logcat", "-b", "main", "-s", "ActivityManager:I", "PackageManager:V"]
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
    
    for line in process.stdout:
        # Detect the 'Start' signature of known bloatware motherships
        if "cmp=" in line:
            for keyword in ["xiaomi", "miui", "sfr", "vodafone", "ironsource"]:
                if keyword in line.lower():
                    pkg = line.split("cmp=")[1].split("/")[0]
                    update_targets(pkg)

if __name__ == "__main__":
    monitor()
