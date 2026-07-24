#!/data/data/com.termux/files/usr/bin/python3
from flask import Flask, render_template_string, jsonify
import logging
import subprocess, os, time

app = Flask(__name__)
log = logging.getLogger(__name__)

DAEMONS = ['rocksteadyd', 'krangd', 'splinterd', 'bebopd', 'leatherheadd',
           'metalheadd', 'ratkingd', 'shredderd', 'tigerclawd', 'granitord',
           'foot_clan_supreme', 'sysportd', 'powerhouse', 'flip_switch']
DB_PATH = "logs/syndicate_footclan.db"

HTML = '''
<!DOCTYPE html>
<html>
<head>
    <title>MiuiserPeruser • Dashboard</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600&display=swap');
        body { font-family: 'Inter', sans-serif; background: #0a0a0a; color: #00ff41; margin:0; padding:20px; }
        .header { text-align:center; margin-bottom:30px; }
        .card { background:#111; border:1px solid #00ff41; border-radius:8px; padding:20px; margin-bottom:20px; }
        .status { display:inline-block; padding:4px 12px; border-radius:9999px; font-size:0.85rem; }
        .on { background:#00ff41; color:#000; }
        .off { background:#ff0000; color:#fff; }
        button { background:#00ff41; color:#000; border:none; padding:12px 24px; border-radius:6px; font-weight:600; cursor:pointer; margin:5px; }
        button:hover { background:#00cc33; }
        pre { background:#1a1a1a; padding:15px; border-radius:6px; overflow:auto; max-height:300px; }
        .layer { margin-top:15px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>MiuiserPeruser Dashboard</h1>
        <p>Live control • All layers • GDPR ready</p>
    </div>

    <div class="card">
        <h3>System Status</h3>
        <pre id="status">Loading...</pre>
    </div>

    <div class="card">
        <h3>Layer Controls</h3>
        <button onclick="toggleGranitor()">Toggle GranitorD Killing</button>
        <button onclick="runScan()">Run Superhero Full Scan</button>
        <button onclick="gdprExport()">GDPR / SAR Export</button>
    </div>

    <div class="card">
        <h3>Recent Logs</h3>
        <pre id="logs">Loading...</pre>
    </div>

    <script>
        function refresh() {
            fetch('/status').then(r => r.text()).then(t => document.getElementById('status').innerHTML = t);
            fetch('/logs').then(r => r.text()).then(t => document.getElementById('logs').innerHTML = t);
        }
        function toggleGranitor() { fetch('/toggle-granitor'); refresh(); }
        function runScan() { fetch('/scan'); refresh(); }
        function gdprExport() { window.location.href = '/gdpr'; }
        setInterval(refresh, 3000);
        refresh();
    </script>
</body>
</html>
'''

@app.route('/')
def home():
    return render_template_string(HTML)

@app.route('/status')
def status():
    try:
        out = subprocess.check_output(['ps', 'aux'], text=True)
    except (OSError, subprocess.CalledProcessError) as e:
        # A bare except here reported "Error reading status" for everything,
        # including a genuinely empty daemon list.
        log.exception("ps aux failed")
        return f"Error reading status: {e}", 500
    return '<br>'.join(line for line in out.splitlines()
                       if any(d in line for d in DAEMONS))

@app.route('/logs')
def logs():
    # shell=True with a list argv silently ran only 'tail', ignoring the
    # glob and the line count.
    try:
        return subprocess.check_output("tail -n 30 logs/*.log",
                                       text=True, shell=True,
                                       stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        log.warning("tail failed (%s): %s", e.returncode, e.output)
        return f"No logs yet: {e.output}", 404
    except OSError as e:
        log.exception("tail failed")
        return f"Error reading logs: {e}", 500

@app.route('/toggle-granitor')
def toggle_granitor():
    try:
        subprocess.run(
            ["sqlite3", DB_PATH,
             "UPDATE heartbeats SET status_flag = (status_flag + 1) % 2 "
             "WHERE daemon_name='granitor_killing_enabled';"],
            check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as e:
        # The UI reported "Toggled" whether or not the update ran.
        log.exception("granitor toggle failed")
        return f"Toggle failed: {e}", 500
    return "Toggled"

@app.route('/scan')
def scan():
    try:
        subprocess.Popen(["nohup", "bin/tigerclawd"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as e:
        log.exception("tigerclawd launch failed")
        return f"Scan failed to start: {e}", 500
    return "Scan started"

def _section(cmd):
    """Run a shell command for the export, keeping failures in the export.

    subprocess.getoutput() merges stderr into stdout and drops the exit
    status, so a failed section was indistinguishable from an empty one.
    """
    result = subprocess.run(cmd, shell=True, text=True, capture_output=True)
    if result.returncode != 0:
        log.warning("export section failed (%s): %s", cmd, result.stderr.strip())
        return (f"[unavailable: command exited {result.returncode}]\n"
                f"{result.stderr.strip()}\n")
    return result.stdout

@app.route('/gdpr')
def gdpr():
    ps_filter = '|'.join(DAEMONS)
    try:
        with open("gdpr_export.txt", "w") as f:
            f.write("=== GDPR / SUBJECT ACCESS REQUEST EXPORT ===\n")
            f.write(f"Generated: {time.ctime()}\n\n")
            f.write("=== RUNNING PROCESSES ===\n")
            f.write(_section(f"ps aux | grep -E '{ps_filter}'"))
            f.write("\n\n=== HEARTBEATS ===\n")
            f.write(_section(f"sqlite3 {DB_PATH} 'SELECT * FROM heartbeats;'"))
            f.write("\n\n=== RECENT LOGS ===\n")
            f.write(_section("tail -n 100 logs/*.log"))
    except OSError as e:
        log.exception("GDPR export failed")
        return f"GDPR export failed: {e}", 500
    return "GDPR export ready. Download gdpr_export.txt from your files."

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
