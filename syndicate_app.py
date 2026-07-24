#!/data/data/com.termux/files/usr/bin/python3
from flask import Flask, abort, render_template_string, request
import glob
import hmac
import os
import secrets
import subprocess
import time

app = Flask(__name__)

DAEMONS = ['rocksteadyd', 'krangd', 'splinterd', 'bebopd', 'leatherheadd',
           'metalheadd', 'ratkingd', 'shredderd', 'tigerclawd', 'granitord',
           'foot_clan_supreme', 'sysportd', 'powerhouse', 'flip_switch']

DB_PATH = 'logs/syndicate_footclan.db'
LOG_GLOB = 'logs/*.log'

BIND_HOST = os.environ.get('SYNDICATE_DASH_HOST', '127.0.0.1')
BIND_PORT = int(os.environ.get('SYNDICATE_DASH_PORT', '5000'))

# The dashboard controls privileged daemons, so every request must carry a
# token. A per-run token is generated when none is supplied via the
# environment.
AUTH_TOKEN = os.environ.get('SYNDICATE_DASH_TOKEN') or secrets.token_urlsafe(32)


@app.before_request
def require_token():
    supplied = request.headers.get('X-Syndicate-Token') or request.args.get('token', '')
    if not hmac.compare_digest(supplied, AUTH_TOKEN):
        abort(401)

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
        const TOKEN = new URLSearchParams(window.location.search).get('token') || '';
        function api(path, opts) {
            return fetch(path, Object.assign({headers: {'X-Syndicate-Token': TOKEN}}, opts || {}));
        }
        function refresh() {
            api('/status').then(r => r.text()).then(t => document.getElementById('status').textContent = t);
            api('/logs').then(r => r.text()).then(t => document.getElementById('logs').textContent = t);
        }
        function toggleGranitor() { api('/toggle-granitor', {method: 'POST'}).then(refresh); }
        function runScan() { api('/scan', {method: 'POST'}).then(refresh); }
        function gdprExport() { api('/gdpr', {method: 'POST'}).then(r => r.text()).then(alert); }
        setInterval(refresh, 3000);
        refresh();
    </script>
</body>
</html>
'''

@app.route('/')
def home():
    return render_template_string(HTML)

def daemon_status():
    out = subprocess.check_output(['ps', 'aux'], text=True)
    return '\n'.join(line for line in out.splitlines()
                     if any(d in line for d in DAEMONS))


def tail_logs(lines):
    paths = sorted(glob.glob(LOG_GLOB))
    if not paths:
        return "No logs yet"
    return subprocess.check_output(['tail', '-n', str(lines)] + paths, text=True)


@app.route('/status')
def status():
    try:
        return daemon_status()
    except (OSError, subprocess.SubprocessError):
        return "Error reading status"

@app.route('/logs')
def logs():
    try:
        return tail_logs(30)
    except (OSError, subprocess.SubprocessError):
        return "No logs yet"

@app.route('/toggle-granitor', methods=['POST'])
def toggle_granitor():
    subprocess.run(["sqlite3", DB_PATH, "UPDATE heartbeats SET status_flag = (status_flag + 1) % 2 WHERE daemon_name='granitor_killing_enabled';"], check=False)
    return "Toggled"

@app.route('/scan', methods=['POST'])
def scan():
    subprocess.Popen(["nohup", "bin/tigerclawd"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return "Scan started"

@app.route('/gdpr', methods=['POST'])
def gdpr():
    fd = os.open("gdpr_export.txt", os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write("=== GDPR / SUBJECT ACCESS REQUEST EXPORT ===\n")
        f.write(f"Generated: {time.ctime()}\n\n")
        f.write("=== RUNNING PROCESSES ===\n")
        f.write(daemon_status())
        f.write("\n\n=== HEARTBEATS ===\n")
        f.write(subprocess.run(["sqlite3", DB_PATH, "SELECT * FROM heartbeats;"],
                               text=True, capture_output=True, check=False).stdout)
        f.write("\n\n=== RECENT LOGS ===\n")
        f.write(tail_logs(100))
    return "GDPR export ready. Download gdpr_export.txt from your files."

if __name__ == '__main__':
    if not os.environ.get('SYNDICATE_DASH_TOKEN'):
        print(f"[Syndicate] Dashboard token: {AUTH_TOKEN}")
        print(f"[Syndicate] Open http://{BIND_HOST}:{BIND_PORT}/?token={AUTH_TOKEN}")
    app.run(host=BIND_HOST, port=BIND_PORT, debug=False)
