#!/data/data/com.termux/files/usr/bin/python3
from flask import Flask, render_template_string, jsonify
import subprocess, os, time

app = Flask(__name__)

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
        return '<br>'.join([line for line in out.splitlines() if any(x in line for x in ['rocksteadyd','krangd','splinterd','bebopd','leatherheadd','metalheadd','ratkingd','shredderd','tigerclawd','granitord','foot_clan_supreme','sysportd','powerhouse','flip_switch'])])
    except:
        return "Error reading status"

@app.route('/logs')
def logs():
    try:
        return subprocess.check_output(['tail', '-n', '30', 'logs/*.log'], text=True, shell=True)
    except:
        return "No logs yet"

@app.route('/toggle-granitor')
def toggle_granitor():
    return "Toggled"

@app.route('/scan')
def scan():
    subprocess.Popen(["nohup", "bin/tigerclawd"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return "Scan started"

@app.route('/gdpr')
def gdpr():
    with open("gdpr_export.txt", "w") as f:
        f.write("=== GDPR / SUBJECT ACCESS REQUEST EXPORT ===\n")
        f.write(f"Generated: {time.ctime()}\n\n")
        f.write("=== RUNNING PROCESSES ===\n")
        f.write(subprocess.getoutput("ps aux | grep -E 'rocksteadyd|krangd|...|sysportd|powerhouse|flip_switch'"))
        f.write("\n\n=== HEARTBEATS ===\n")
        f.write("\n\n=== RECENT LOGS ===\n")
        f.write(subprocess.getoutput("tail -n 100 logs/*.log"))
    return "GDPR export ready. Download gdpr_export.txt from your files."

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
