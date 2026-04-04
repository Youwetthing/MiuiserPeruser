import sys, os

class ReaperDaemon:
    def __init__(self, sewer):
        self.sewer = sewer

    def identify_traitor(self):
        # Using 'ps' to find top CPU consumers - more reliable than 'top' on some kernels
        cmd = "ps -A -o %CPU,CMD --sort=-%cpu | head -n 2"
        raw = self.sewer.query(cmd)
        
        if raw:
            lines = raw.strip().split('\n')
            # The first line is usually the header, second is the traitor
            if len(lines) > 1:
                return lines[1].strip()
        return "Analyzing Shadow..."
