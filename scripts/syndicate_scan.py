#!/usr/bin/env python3
"""
syndicate_scan.py – Working parallel scan (no shell timeout, verbose errors)
"""

import subprocess, json, os, sys, time
from pathlib import Path

BASE = Path.home() / "MiuiserPeruser"
BIN = BASE / "bin"
RESULTS = BASE / "Registry" / "daemon_results"
LOGS = BASE / "logs"
RISH = Path.home() / "rish"

DAEMONS = {
    "granitord": 120, "burned": 30, "leatherheadd": 30, "metalheadd": 30,
    "rahzerd": 60, "ratkingd": 60, "rocksteadyd": 30, "shredderd": 60,
    "tigerclawd": 60, "bebopd": 30, "fugitoidd": 60, "overlordd": 30,
    "nulld": 30
}

WHAT = {
    "burned": "Your phone has tracking code baked into it at factory level by Xiaomi.",
    "granitord": "Checks the security foundations of your device.",
    "leatherheadd": "Monitors thermal throttling – your phone's cooling system.",
    "metalheadd": "Watches for background sensor access – potential tracking.",
    "rahzerd": "Monitors all network connections – WiFi, mobile, DNS.",
    "ratkingd": "Watches memory and zombie processes.",
    "rocksteadyd": "Monitors CPU throttling – processor stress levels.",
    "shredderd": "Checks kernel integrity – looks for rootkits.",
    "tigerclawd": "HyperOS insider – flags system drift.",
    "bebopd": "Measures battery drain rate when idle.",
    "fugitoidd": "Reads crash logs and ANRs.",
    "overlordd": "Cross-daemon correlation engine.",
    "nulld": "Watches for idle network spikes – covert transmissions."
}

TECH = {
    "burned": "Facebook/AppsFlyer partner IDs at ROM level.",
    "granitord": "SELinux, verified boot, dm-verity, 7 integrity vectors.",
    "leatherheadd": "MTK thermal zones, scaling_cur_freq deltas.",
    "metalheadd": "/proc/[pid]/fd sensor nodes, ACCELEROMETER/GYROSCOPE.",
    "rahzerd": "Telephony registry, getaddrinfo DNS, /proc/net/tcp.",
    "ratkingd": "/proc status zombie scan, MemAvailable pressure.",
    "rocksteadyd": "scaling_cur_freq throttle detection.",
    "shredderd": "/proc/modules vs baseline DJB2, rootkit sig scan.",
    "tigerclawd": "Binder topology, ro.* hash drift, 6 vectors.",
    "bebopd": "charge_now delta, current_now, mAh/hr calculation.",
    "fugitoidd": "Logcat FATAL, ANR, OOM kill parsing.",
    "overlordd": "Thermal+CPU+battery = hardware; network+binder+memory = anomaly.",
    "nulld": "Screen state polling, netstats spike detection."
}

RESULTS.mkdir(parents=True, exist_ok=True)
LOGS.mkdir(parents=True, exist_ok=True)

def extract_first_json(text):
    """Find the first JSON object in a string."""
    start = text.find('{')
    if start == -1:
        return None
    for end in range(len(text), start, -1):
        try:
            return json.loads(text[start:end])
        except json.JSONDecodeError:
            continue
    return None

def launch_daemon(name, timeout_sec):
    bin_path = BIN / name
    cmd = f"{bin_path}"
    if name == "granitord":
        cmd = f"export GRANITORD_POLL_SEC=3; {cmd}"
    if name == "fugitoidd":
        cmd = f"export BEXEC_NO_RISH=1; {cmd}"

    env = os.environ.copy()
    env["RISH_APPLICATION_ID"] = "com.termux"

    try:
        proc = subprocess.Popen(
            [str(RISH), "-c", cmd],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=env,
            text=True
        )
        return proc
    except Exception as e:
        print(f"  [FAIL] {name}: {e}")
        return None

def summary_line(name, data):
    try:
        if name == "burned":
            return f"{data.get('privacy_signal_count', 0)} privacy signals"
        elif name == "granitord":
            p = data.get("posture", {})
            return f"Posture {p.get('score','?')}/100 ({p.get('grade','?')})"
        elif name == "leatherheadd":
            return f"Thermal {data.get('thermal_score','?')}/100, {data.get('throttled_cores',0)} cores throttled"
        elif name == "metalheadd":
            return f"{data.get('sensitive_active',0)} sensitive sensors active"
        elif name == "rahzerd":
            ports = data.get("ports", {})
            dns = data.get("dns", {})
            return f"{ports.get('established_tcp4',0)} TCP4 / {ports.get('established_tcp6',0)} TCP6, DNS {dns.get('latency_ms','?')}ms"
        elif name == "ratkingd":
            proc = data.get("processes", {})
            mem = data.get("memory", data.get("pressure", {}))
            return f"{proc.get('zombies',0)} zombies, {mem.get('available_mb','?')} MB free"
        elif name == "rocksteadyd":
            return f"CPU {data.get('cpu_score','?')}/100, {data.get('throttled_cores',0)} cores throttled"
        elif name == "shredderd":
            integ = data.get("integrity", {})
            new_mods = len(data.get("drift", {}).get("new_modules", []))
            return f"Integrity {integ.get('score','?')}/100, {new_mods} new modules"
        elif name == "tigerclawd":
            binder = data.get("binder", {})
            return f"Trust {data.get('trust_score','?')}/100, drift {binder.get('drift',0)}"
        elif name == "bebopd":
            return f"Drain {data.get('drain_mah_h', data.get('drain_mah_per_hour','?'))} mAh/hr (Grade {data.get('grade','?')})"
        elif name == "fugitoidd":
            c = data.get("crashes", data.get("crash_count", 0))
            a = data.get("anrs", data.get("anr_count", 0))
            o = data.get("oom_events", data.get("oom_count", 0))
            return f"{c} crashes, {a} ANRs, {o} OOMs"
        elif name == "overlordd":
            corr = data.get("correlation", {})
            return f"{corr.get('status','?')}, {corr.get('anomaly_count',0)} anomalies"
        elif name == "nulld":
            return f"Spike: {data.get('idle_spike_detected',False)}, {data.get('total_spike_events',0)} events (screen {data.get('screen','?')})"
        else:
            return "result parsed"
    except:
        return "parse error"

def run_scan():
    print("╔══════════════════════════════════════╗")
    print("║   SYNDICATE SCAN (PARALLEL)         ║")
    print("╚══════════════════════════════════════╝\n")

    procs = {}
    for name, timeout in DAEMONS.items():
        if not (BIN / name).exists():
            print(f"  [SKIP] {name} - binary missing")
            continue
        print(f"  [LAUNCH] {name} (timeout {timeout}s)")
        proc = launch_daemon(name, timeout)
        if proc:
            procs[name] = proc

    print(f"\n  Waiting for {len(procs)} daemons...\n")

    results = {}
    for name, proc in procs.items():
        try:
            out, err = proc.communicate(timeout=DAEMONS[name])
        except subprocess.TimeoutExpired:
            proc.kill()
            out, err = proc.communicate()
            print(f"  [TIMEOUT] {name} killed after {DAEMONS[name]}s")
        except Exception as e:
            print(f"  [ERROR] {name}: {e}")
            out, err = "", ""

        # Save raw output to file
        result_file = RESULTS / f"{name}.json"
        with open(result_file, 'w') as f:
            f.write(out)
        with open(LOGS / f"{name}.log", 'a') as f:
            f.write(err)

        data = None
        if out.strip():
            data = extract_first_json(out)
            if data:
                print(f"  [DONE] {name}: {summary_line(name, data)}")
            else:
                # Try stderr
                data = extract_first_json(err)
                if data:
                    print(f"  [DONE] {name} (stderr): {summary_line(name, data)}")
                else:
                    print(f"  [WARN] {name}: no JSON in output.")
                    print(f"    stdout (first 100): {out[:100]}")
                    print(f"    stderr (first 100): {err[:100]}")
        else:
            print(f"  [FAIL] {name}: empty output. stderr: {err[:100]}")
        results[name] = data

    print("\n" + "="*50)
    print("        DETAILED REPORT")
    print("="*50)
    for name, data in results.items():
        if data is None:
            print(f"\n  {name}: no data\n")
            continue
        print(f"\n  >> {name.upper()}")
        print(f"    WHAT THIS MEANS:")
        print(f"    {WHAT.get(name, 'No description.')}")
        print(f"\n    TECHNICAL:")
        print(f"    {TECH.get(name, 'No technical details.')}")
        if name == "burned":
            print(f"    Privacy signals: {data.get('privacy_signal_count')}")
            ids = [p['value'] for p in data.get('privacy_props', []) if 'partnerid' in p.get('key','') or 'channel' in p.get('key','')]
            if ids:
                print(f"    Partner IDs: {', '.join(ids)}")
        elif name == "granitord":
            p = data.get("posture", {})
            print(f"    SELinux: {p.get('selinux')}")
            print(f"    Verified Boot: {p.get('verified_boot')}")
            print(f"    Score: {p.get('score')}/100 ({p.get('grade')})")
        elif name == "shredderd":
            integ = data.get("integrity", {})
            print(f"    Integrity score: {integ.get('score')}")
            new_mods = data.get("drift", {}).get("new_modules", [])
            print(f"    New kernel modules: {new_mods}")
        elif name == "tigerclawd":
            print(f"    Trust score: {data.get('trust_score')}")
            dev = data.get("device", {})
            print(f"    HyperOS version: {dev.get('hyperos_version', '?')}")
            print(f"    Security patch: {dev.get('security_patch', '?')}")
        elif name == "bebopd":
            print(f"    Drain: {data.get('drain_mah_h', data.get('drain_mah_per_hour'))} mAh/hr")
            print(f"    Grade: {data.get('grade')}")

    print("\n  Scan complete.")

if __name__ == "__main__":
    run_scan()
