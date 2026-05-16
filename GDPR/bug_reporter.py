import os
import json
import re
from datetime import datetime

class BugReporter:
    def __init__(self, base_dir=None):
        if base_dir is None:
            current_dir = os.path.dirname(os.path.abspath(__file__))
            self.base_dir = os.path.abspath(os.path.join(current_dir, '..'))
        else:
            self.base_dir = os.path.abspath(target_dir)

        self.anonymized_map = {}
        self.id_counter = 1
        self.exclude_dirs = {'.git', 'bin', 'out', 'build', '__pycache__', '_legacy_bin', 'obj'}
        self.log_extensions = ('.log', '.events', '.state', '.registry', '.txt', '.rules', '.cfg', '.pid')

    def _should_collect(self, file_path):
        if file_path.endswith(self.log_extensions):
            return True
        # Also catch specific files without extensions or with hidden names
        name = os.path.basename(file_path)
        if name.startswith('.') or 'registry' in name or 'ledger' in name:
            return True
        return False

    def generate_report(self):
        report = {
            "type": "MiuiserPeruser_RepoWide_Anonymized_Debug_Report",
            "timestamp": datetime.now().isoformat(),
            "repo_root": self.base_dir,
            "anonymized_data": []
        }

        files_to_process = []
        for root, dirs, files in os.walk(self.base_dir):
            dirs[:] = [d for d in dirs if d not in self.exclude_dirs]
            for file in files:
                file_path = os.path.abspath(os.path.join(root, file))
                if self._should_collect(file_path):
                    files_to_process.append(file_path)

        # First pass: Build anonymization map
        # We look for the most common identifiers in pipe-delimited logs
        for file_path in files_to_process:
            try:
                with open(file_path, 'r', errors='ignore') as f:
                    for line in f:
                        parts = line.strip().split('|')
                        if len(parts) >= 2:
                            subject = parts[1]
                            # Don't anonymize system constants
                            if subject not in self.anonymized_map and \
                               subject not in ["JUDGE", "ORCHESTRATOR", "judicial_controller", "CORE", "CRE", "STRESS", "ORCHESTRATOR", "HEARTBEAT", "STATE", "STARTED", "STOPPED", "QUARANTINED", "JAILED"]:
                                if len(subject) > 2: # Avoid tiny strings
                                    self.anonymized_map[subject] = f"app_{self.id_counter}"
                                    self.id_counter += 1
            except: pass

        # Second pass: Collect and Scrub
        for file_path in files_to_process:
            file_data = {
                "file": os.path.relpath(file_path, self.base_dir),
                "entries": []
            }
            try:
                with open(file_path, 'r', errors='ignore') as f:
                    for line in f:
                        content = line.strip()
                        for real_id, anon_id in self.anonymized_map.items():
                            content = content.replace(real_id, anon_id)
                        file_data["entries"].append(content)
                report["anonymized_data"].append(file_data)
            except: pass

        return report

if __name__ == '__main__':
    reporter = BugReporter()
    data = reporter.generate_report()
    
    export_dir = "/sdcard/Documents"
    if not os.path.exists(export_dir):
        export_dir = os.path.expanduser("~/storage/downloads")
        if not os.path.exists(export_dir):
            export_dir = os.path.join(reporter.base_dir, "GDPR/exports")
            os.makedirs(export_dir, exist_ok=True)
            
    filename = f"FullRepo_DebugReport_ANONYMIZED_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
    path = os.path.join(export_dir, filename)
    
    with open(path, 'w') as f:
        json.dump(data, f, indent=2)
        
    print(f"[Syndicate] Repo-wide anonymized debug report generated.")
    print(f"[Syndicate] Files analyzed: {len(data['anonymized_data'])}")
    print(f"[Syndicate] Exported to: {path}")
