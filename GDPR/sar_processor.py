import os
import json
import re
from datetime import datetime
import shutil

class SARProcessor:
    def __init__(self, target_dir=None):
        if target_dir is None:
            current_dir = os.path.dirname(os.path.abspath(__file__))
            self.base_dir = os.path.abspath(os.path.join(current_dir, '..'))
        else:
            self.base_dir = os.path.abspath(target_dir)

        self.data_sources = {
            'court_events': 'state/court.events',
            'criminal_ledger': 'state/criminal_record/ledger.log',
            'jailhouse_registry': 'state/jailhouse/registry',
            'court_registry': 'state/court.registry',
            'daemon_registry': 'state/daemon.registry',
            'visitors_pass': 'state/visitors_pass/pass_registry',
            'quarantine_state': 'state/quarantine.state'
        }

    def process_request(self, subject_id, recursive=True):
        report = {
            'subject_id': subject_id,
            'timestamp': datetime.now().isoformat(),
            'target_directory': self.base_dir,
            'findings': []
        }
        for source_key, rel_path in self.data_sources.items():
            file_path = os.path.join(self.base_dir, rel_path)
            if os.path.exists(file_path):
                self._scan_file(file_path, subject_id, source_key, report)
        if recursive:
            exclude_dirs = {'.git', 'bin', 'out', 'build', '__pycache__', '_legacy_bin', 'obj'}
            priority_paths = set(os.path.abspath(os.path.join(self.base_dir, f)) for f in self.data_sources.values())
            for root, dirs, files in os.walk(self.base_dir):
                dirs[:] = [d for d in dirs if d not in exclude_dirs]
                for file in files:
                    file_path = os.path.abspath(os.path.join(root, file))
                    if file_path in priority_paths: continue
                    if not file.endswith(('.o', '.bin', '.exe', '.so', '.a', '.pyc', '.dex')):
                        self._scan_file(file_path, subject_id, 'repo_wide_scan', report)
        return report

    def _scan_file(self, file_path, subject_id, source_key, report):
        if not os.path.exists(file_path) or os.path.isdir(file_path): return
        try:
            with open(file_path, 'r', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    if subject_id in line:
                        report['findings'].append({
                            'source': source_key,
                            'file': os.path.relpath(file_path, self.base_dir),
                            'line': line_num,
                            'content': line.strip(),
                            'parsed_data': self._parse_line(line, source_key)
                        })
        except: pass

    def erasure_request(self, subject_id, anonymize=True):
        report = self.process_request(subject_id, recursive=True)
        affected_files = set(f['file'] for f in report['findings'])
        for rel_path in affected_files:
            file_path = os.path.join(self.base_dir, rel_path)
            temp_path = file_path + ".tmp"
            with open(file_path, 'r') as fin, open(temp_path, 'w') as fout:
                for line in fin:
                    if subject_id in line:
                        if anonymize: fout.write(line.replace(subject_id, "[ANONYMIZED]"))
                        else: continue
                    else: fout.write(line)
            os.replace(temp_path, file_path)
        return {"status": "success", "subject_id": subject_id, "action": "anonymized" if anonymize else "deleted", "files_affected": list(affected_files)}

    def _parse_line(self, line, source_key):
        parts = line.strip().split('|')
        parsed = {}
        try:
            if source_key in ['court_events', 'criminal_ledger'] or len(parts) >= 3:
                if len(parts) >= 1 and parts[0].isdigit():
                    parsed['timestamp_raw'] = parts[0]
                    try: parsed['timestamp_human'] = datetime.fromtimestamp(int(parts[0])).isoformat()
                    except: pass
                if len(parts) >= 2: parsed['subject'] = parts[1]
                if len(parts) >= 3: parsed['event'] = parts[2]
        except: pass
        return parsed

if __name__ == '__main__':
    import sys
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("subject")
    parser.add_argument("--recursive", action="store_true", default=True)
    parser.add_argument("--erase", action="store_true")
    parser.add_argument("--delete", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    
    processor = SARProcessor()
    if args.erase: results = processor.erasure_request(args.subject, anonymize=not args.delete)
    else: results = processor.process_request(args.subject, recursive=args.recursive)
        
    if args.verbose or args.erase:
        print(json.dumps(results, indent=2))

    if not args.erase:
        export_dir = "/sdcard/Documents"
        if not os.path.exists(export_dir):
            termux_storage = os.path.expanduser("~/storage/downloads")
            if os.path.exists(termux_storage): export_dir = termux_storage
            else:
                export_dir = os.path.join(processor.base_dir, "GDPR/exports")
                os.makedirs(export_dir, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"Audit_{args.subject}_{timestamp}.json"
        export_path = os.path.join(export_dir, filename)
        try:
            with open(export_path, 'w') as f: json.dump(results, f, indent=2)
            if not args.verbose:
                print(f"[Syndicate] Audit complete. Findings: {len(results['findings'])}")
            print(f"[Syndicate] Exported to: {export_path}")
        except Exception as e:
            print(f"[Error] Export failed: {e}")
