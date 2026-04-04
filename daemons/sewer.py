import subprocess
import os

class SewerPipe:
    def __init__(self):
        # The Nuance: Detect if we are in a Rish-enabled environment
        self.rish_path = "/data/data/com.termux/files/usr/bin/rish"
        self.has_rish = os.path.exists(self.rish_path)
        self.tag = "[SEWER-RISH]" if self.has_rish else "[SEWER-ADB]"

    def execute(self, cmd):
        """The core execution logic. Bypasses SELinux via Shizuku if available."""
        try:
            if self.has_rish:
                # Binder-based execution (Silent & Fast)
                full_cmd = ["rish", "-c", cmd]
            else:
                # Fallback to the flaky 127.0.0.1 bridge
                full_cmd = ["adb", "shell", cmd]
            
            result = subprocess.check_output(full_cmd, stderr=subprocess.DEVNULL)
            return result.decode().strip()
        except Exception as e:
            return f"OFFLINE: {str(e)}"

    def get_battery_intel(self):
        # Multi-node read in one rish hit to save overhead
        nodes = "/sys/class/power_supply/battery/capacity /sys/class/power_supply/battery/temp"
        raw = self.execute(f"cat {nodes}")
        lines = raw.split('\n')
        if len(lines) >= 2:
            return {"cap": lines[0], "temp": lines[1]}
        return {"cap": "0", "temp": "0"}
