import os

class LogManager:
    def __init__(self, base_dir=None, max_size_mb=5): # Lowered to 10MB
        if base_dir is None:
            current_dir = os.path.dirname(os.path.abspath(__file__))
            self.base_dir = os.path.abspath(os.path.join(current_dir, '..'))
        else:
            self.base_dir = os.path.abspath(base_dir)
        
        self.max_size = max_size_mb * 1024 * 1024
        self.log_extensions = ('.log', '.events', '.state', '.txt')

    def enforce_caps(self):
        # We only print if we actually find something to cap, to keep it quiet
        for root, dirs, files in os.walk(self.base_dir):
            if any(x in root for x in ['.git', 'bin', 'out', 'build', 'obj']):
                continue
                
            for file in files:
                if file.endswith(self.log_extensions):
                    file_path = os.path.join(root, file)
                    try:
                        size = os.path.getsize(file_path)
                        if size > self.max_size:
                            print(f"[Syndicate] Capping {os.path.relpath(file_path, self.base_dir)} ({size // 1024}KB)")
                            self._truncate_file(file_path)
                    except: pass

    def _truncate_file(self, file_path):
        try:
            # For 10MB cap, we keep the last 2MB of logs
            keep_size = 1 * 1024 * 1024 
            with open(file_path, 'r', errors='ignore') as f:
                f.seek(0, os.SEEK_END)
                end_pos = f.tell()
                f.seek(max(0, end_pos - keep_size))
                recent_data = f.read()
                
            with open(file_path, 'w') as f:
                f.write("--- LOG ROTATED (MAX 5MB) ---\n")
                f.write(recent_data)
        except:
            with open(file_path, 'w') as f:
                f.write("--- LOG CLEARED ---\n")

if __name__ == '__main__':
    manager = LogManager()
    manager.enforce_caps()
