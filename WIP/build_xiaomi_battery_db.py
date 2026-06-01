import re
import requests
from bs4 import BeautifulSoup
import urllib3
import json
import sys

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

HEADERS = {"User-Agent": "Mozilla/5.0"}


# -----------------------------
# PARSE LINE
# -----------------------------
def parse_line(line):
    codename = None
    sku = None

    m = re.search(r"\(\s*(.*?)\s*\)\s*$", line)
    if m:
        codename = m.group(1)

    sku_match = re.findall(r"\b\d{5}[A-Z0-9]{4,}\b", line)
    if sku_match:
        sku = sku_match[-1]

    device = re.sub(r"Supplier:.*?\| Device:", "", line)
    device = re.sub(r"\(.*?\)\s*$", "", device).strip()

    return {
        "device": device,
        "sku": sku,
        "codename": codename
    }


# -----------------------------
# URL BUILDER
# -----------------------------
def build_url(device_name):
    return "https://phonedb.net/index.php?m=device&s=list&search_exp=" + device_name.replace(" ", "+")


# -----------------------------
# BATTERY SCRAPER
# -----------------------------
def get_battery(url):
    try:
        r = requests.get(url, headers=HEADERS, timeout=20, verify=False)
        soup = BeautifulSoup(r.text, "html.parser")

        text = soup.get_text(" ", strip=True)

        match = re.search(r"(\d{3,5})\s?mAh", text)
        if match:
            return match.group(1) + " mAh"

    except Exception:
        return None

    return None


# -----------------------------
# INPUT HANDLER (FIXED)
# -----------------------------
def get_input_lines():
    # 1. If piped input exists
    if not sys.stdin.isatty():
        return [l.strip() for l in sys.stdin.read().splitlines() if l.strip()]

    # 2. fallback empty list (no crash)
    print("⚠️ No input provided. Paste lines or pipe a file.")
    return []


# -----------------------------
# PIPELINE
# -----------------------------
def process(lines):
    results = []

    for i, line in enumerate(lines):
        print(f"[{i}] Processing")

        parsed = parse_line(line)
        url = build_url(parsed["device"])
        battery = get_battery(url)

        parsed["battery"] = battery
        parsed["url"] = url

        print("  →", parsed)
        results.append(parsed)

    return results


# -----------------------------
# SAVE OUTPUT
# -----------------------------
def save_jsonl(data):
    with open("xiaomi_battery.jsonl", "w") as f:
        for item in data:
            f.write(json.dumps(item) + "\n")


# -----------------------------
# RUN
# -----------------------------
if __name__ == "__main__":

    lines = get_input_lines()

    if not lines:
        print("No data → exiting cleanly")
        exit(0)

    results = process(lines)

    save_jsonl(results)

    print("\nDONE → xiaomi_battery.jsonl created")
