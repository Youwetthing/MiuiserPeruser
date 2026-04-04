import subprocess
import time

class SewerPipe:
    """Handles the literal transport of commands to the privileged worker."""
    def __init__(self, adb_id="127.0.0.1:5555"):
        self.base_adb = ["adb", "-s", adb_id, "shell"]
        self.tmp = "/data/local/tmp"

    def query(self, cmd, timeout=2.0):
        # 1. Clear previous state
        subprocess.run(self.base_adb + [f"rm -f {self.tmp}/a.ready"], capture_output=True)
        
        # 2. Hardened Delivery (The EOF pattern we discussed)
        setup_cmd = (
            f"cat << 'EOF' > {self.tmp}/q.tmp\n"
            f"{cmd}\n"
            f"EOF\n"
            f"mv {self.tmp}/q.tmp {self.tmp}/q.ready"
        )
        subprocess.run(self.base_adb + [setup_cmd], capture_output=True)
        
        # 3. Polling for the answer
        start = time.time()
        while (time.time() - start) < timeout:
            check = subprocess.run(self.base_adb + [f"ls {self.tmp}/a.ready"], 
                                   capture_output=True, text=True)
            if "a.ready" in check.stdout:
                # 4. Fetch and Clean
                res = subprocess.run(self.base_adb + [f"cat {self.tmp}/a.ready && rm -f {self.tmp}/a.ready"], 
                                     capture_output=True, text=True)
                return res.stdout.strip()
            time.sleep(0.1)
            
        return None # Timeout or Offline
