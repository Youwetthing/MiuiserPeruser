# SAR Processor & GDPR Compliance Tool

This tool is designed to fulfill Subject Access Requests (SAR) and "Right to Erasure" requests as mandated by the GDPR for the entire MiuiserPeruser repository.

## Features

- **Subject Access Request (SAR):** Extracts all records associated with a specific subject (application name, process name, or identifier) from the repository's logs, registries, and state files.
- **Right to Erasure (Art. 17):** Allows for the anonymization or deletion of all identified records for a given subject.
- **JSON Export:** Automatically exports SAR reports in JSON format to /sdcard/Documents.
- **Automatic Repo-Wide Scanning:** Identifies the repository root and scans all subdirectories, excluding binaries.
- **Comprehensive Coverage:** Scans all text-based files, priority state files, and logs.

## Usage

### 1. Perform a Subject Access Request (SAR)
To find all data related to a subject and export a JSON report:
\`\`\`bash
python3 sar_processor.py <subject_id>
\`\`\`
*Note: This automatically performs a recursive scan and exports to /sdcard/Documents.*

### 2. Execute Right to Erasure (Anonymization)
To replace the subject identifier with [ANONYMIZED] across all files in the repo:
\`\`\`bash
python3 sar_processor.py <subject_id> --erase
\`\`\`

### 3. Execute Right to Erasure (Deletion)
To completely remove lines containing the subject identifier:
\`\`\`bash
python3 sar_processor.py <subject_id> --erase --delete
\`\`\`

## Export Location
Reports are exported to:
- Primary: /sdcard/Documents
- Fallback (Termux): ~/storage/downloads
- Local Fallback: ./exports/

## Installation
The script sar_processor.py and documentation are located in the GDPR/ directory.

---
*Note: This tool is intended for administrative use to ensure regulatory compliance.*
