import subprocess
import time
import os
import sys

class SewerPipe:
    def __init__(self, adb_id="127.0.0.1:5555"):
        self.base_adb = ["adb", "-s", adb_id, "shell"]
        self.tmp = "/data/local/tmp"

    def query(self, cmd, timeout=2.0):
        # Clear previous answer
        subprocess.run(self.base_adb + [f"rm -f {self.tmp}/a.ready"], capture_output=True)
        
        # Drop Question
        setup = f"cat << 'EOF' > {self.tmp}/q.tmp\n{cmd}\nEOF\nmv {self.tmp}/q.tmp {self.tmp}/q.ready"
        subprocess.run(self.base_adb + [setup], capture_output=True)
        
        # Poll for Answer
        start = time.time()
        while (time.time() - start) < timeout:
            check = subprocess.run(self.base_adb + [f"ls {self.tmp}/a.ready"], capture_output=True, text=True)
            if "a.ready" in check.stdout:
                res = subprocess.run(self.base_adb + [f"cat {self.tmp}/a.ready && rm -f {self.tmp}/a.ready"], capture_output=True, text=True)
                return res.stdout.strip()
            time.sleep(0.1)
        return None
