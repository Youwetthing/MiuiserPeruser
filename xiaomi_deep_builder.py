import requests
from bs4 import BeautifulSoup
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer
from reportlab.lib.styles import getSampleStyleSheet
import time
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE = "https://phonedb.net"
HEADERS = {"User-Agent": "Mozilla/5.0"}

# -------------------------
# GET DEVICE LINKS
# -------------------------
def get_device_links(offset):
    url = f"{BASE}/index.php?m=device&s=list&search_exp=Xiaomi&order_field=brand&filter={offset}"

    try:
        r = requests.get(url, headers=HEADERS, verify=False, timeout=20)
        soup = BeautifulSoup(r.text, "html.parser")

        links = []

        for a in soup.find_all("a", href=True):
            href = a["href"]

            if "m=device&s=specs&id=" in href:
                full = BASE + "/" + href
                links.append(full)

        return list(set(links))

    except Exception as e:
        print(f"[!] List page fail {offset}: {e}")
        return []


# -------------------------
# PARSE DEVICE PAGE
# -------------------------
def parse_device(url):
    try:
        r = requests.get(url, headers=HEADERS, verify=False, timeout=20)
        soup = BeautifulSoup(r.text, "html.parser")

        data = {
            "name": "Unknown",
            "battery": "Unknown",
            "codename": "Unknown"
        }

        # Device name
        title = soup.find("h1")
        if title:
            raw = title.get_text(strip=True)

            # Clean naming (your rule)
            raw = raw.replace("Xiaomi", "").strip()
            data["name"] = raw

        # Specs table parsing
        for row in soup.find_all("tr"):
            text = row.get_text(" ", strip=True)

            if "Battery" in text:
                data["battery"] = text.replace("Battery", "").strip()

            if "Model" in text or "Codename" in text:
                data["codename"] = text.replace("Model", "").replace("Codename", "").strip()

        return data

    except Exception as e:
        print(f"[!] Device fail: {e}")
        return None


# -------------------------
# MAIN BUILD
# -------------------------
def build_dataset():
    seen = set()
    dataset = []

    for offset in range(0, 1567, 29):
        print(f"[+] Offset {offset}")

        links = get_device_links(offset)

        for link in links:
            if link in seen:
                continue

            seen.add(link)

            data = parse_device(link)
            if data:
                dataset.append(data)

            time.sleep(0.4)

    return dataset


# -------------------------
# PDF EXPORT
# -------------------------
def export_pdf(data):
    doc = SimpleDocTemplate("xiaomi_full_report.pdf")
    styles = getSampleStyleSheet()

    content = []
    content.append(Paragraph("Xiaomi Full Device Intelligence Report", styles["Title"]))
    content.append(Spacer(1, 12))

    for d in data:
        block = f"""
        <b>Supplier:</b> Xiaomi<br/>
        <b>Device:</b> {d['name']}<br/>
        <b>Battery:</b> {d['battery']}<br/>
        <b>Codename:</b> {d['codename']}<br/>
        """

        content.append(Paragraph(block, styles["Normal"]))
        content.append(Spacer(1, 10))

    doc.build(content)
    print("PDF CREATED → xiaomi_full_report.pdf")


# -------------------------
# RUN
# -------------------------
if __name__ == "__main__":
    data = build_dataset()
    print(f"TOTAL DEVICES: {len(data)}")
    export_pdf(data)
