#!/usr/bin/env python3
"""apply_findings.py - apply verified supplier/battery findings to a DB built by build_devices_db.py.

    python3 apply_findings.py --in Registry/xiaomi_devices.db --out Registry/xiaomi_devices_verified.db

Re-runnable: always starts from --in, never edits it. Extra tables (suppliers, evidence, eprel) are added;
the builder's own tables/view are left intact. Only values a source actually stated are written.
"""
import argparse, os, shutil, sqlite3, stat, hashlib, time

TODAY = "2026-11-07"
FAQ = "https://www.mi.com/global/support/faq/details/"

# ---------------------------------------------------------------- suppliers
# (name, aliases, supplier_group, status, note)
SUPPLIERS = [
    ("ATL", "Amperex Technology Limited|Amperex", "ATL", "confirmed",
     "TDK subsidiary. No Sunwoda ownership found."),
    ("Sunwoda", "Sunwoda Electronic", "Sunwoda", "confirmed",
     "Pack/module maker; Xiaomi FAQ names it as pack supplier (Mi 15). Parent of LIWINON."),
    ("LIWINON", "LWN|LW|Liwinon|Dongguan Liwinon", "Sunwoda", "confirmed",
     "Wholly-owned Sunwoda subsidiary since 2018 (per liwinon.com). Cell maker."),
    ("COSMX", "COS|Coslight|Zhuhai Coslight|CosMX", "COSMX", "confirmed",
     "Formerly Zhuhai Coslight Battery; rebranded 2019. No Sunwoda acquisition found; full shareholder list not seen."),
    ("Everup", "Everup Battery India", "Everup", "lead_unverified",
     "Only a 2021 forum post (XDA) names it: vendor for India-assembled devices. No Xiaomi source."),
    ("Desay", "Huizhou Desay", "Desay", "unverified_for_xiaomi",
     "Real maker (Apple UN38.3 list). Only seller copy links it to Redmi."),
    ("CATL", "", "CATL", "rejected_for_phones",
     "EV supplier to Xiaomi cars (SU7). Earlier heuristic hints for phones rejected."),
    ("BYD", "", "BYD", "rejected_for_phones", "EV supplier to Xiaomi cars; no phone link found."),
    ("CALB", "", "CALB", "rejected_for_phones", "EV supplier (Skynomad sub-brand, 2026 report); no phone link found."),
    ("Longhehui", "", "Longhehui", "rejected", "Aftermarket replacement-battery seller, not an OEM supplier."),
]

# ---------------------------------------------------------------- spec updates
# (record, device_name, typ, rated, watt, supplier, source_url, notes, needs_review)
SPECS = [
    (55, "Mi 11", 4600, 4500, None, "Sunwoda",
     "https://brodos.com/wp-content/uploads/2021/03/Xiaomi-Mi-11-UN38.3.pdf",
     "UN38.3 report S-20070127A0: battery BM4X 3.87V, 4500mAh/17.4Wh rated, 4600mAh/17.8Wh typ; client and factory Sunwoda. Charge wattage not in report.", 0),
    (77, "Mi 13 Pro", 4820, None, 120, None, FAQ + "KA-12717/",
     "Global FAQ: 4820mAh typ, 120W wired. No supplier stated; ATL hint (TechInsights via NotebookLM) not verified.", 1),
    (81, "Mi 14", 4610, None, 90, "ATL|LIWINON", FAQ + "KA-243623/",
     "FAQ wording 'ATL/LW' (LW read as LIWINON, unconfirmed). 4610 is TYPICAL only; earlier 'rated 4610' was wrong.", 0),
    (83, "Mi 14 Ultra", 5000, None, 90, "ATL|LIWINON", FAQ + "KA-108264/",
     "FAQ wording 'ATL/LWN'. Global 5000 typ; China variant 5300 (Wikipedia). 4.5V high-voltage cell.", 0),
    (85, "Mi 14T Pro", 5000, None, 120, "ATL", FAQ + "KA-491239/",
     "FAQ: 5000mAh typ, supplied by ATL. 120W per GSMArena/DXOMARK, not the FAQ.", 0),
    (86, "Mi 15", 5240, None, 90, "LIWINON", FAQ + "KA-541914/",
     "FAQ: battery (pack) supplied by Sunwoda, cell by LIWINON (Sunwoda subsidiary). Global 5240; China 5400.", 0),
    (88, "Mi 15 Ultra", 5410, None, None, None, "https://en.wikipedia.org/wiki/Xiaomi_15",
     "Global 5410, China 6000 (Wikipedia, not Xiaomi). Earlier CATL hint rejected; no supplier found.", 1),
    (91, "Mi 15T Pro", 5500, 5330, 90, "LIWINON", FAQ + "KA-608137/",
     "FAQ: dual-cell, 5330 rated/5500 typ, 4.53V limit. >=80% after 1600 cycles is a Xiaomi internal-lab claim.", 0),
    (182, "Redmi 13C", 5000, None, 18, "LIWINON|ATL|COSMX", "https://www.mi.com/uk/support/faq/details/KA-242749/",
     "FAQ for REDMI 13C (4G/5G not specified): 18W fast charge, 10W standard charger.", 1),
    (183, "Redmi 13C", 5000, None, 18, "LIWINON|ATL|COSMX", "https://www.mi.com/uk/support/faq/details/KA-242749/",
     "FAQ for REDMI 13C (4G/5G not specified): 18W fast charge, 10W standard charger.", 1),
    (209, "Redmi 13C", 5000, None, 18, "LIWINON|ATL|COSMX", "https://www.mi.com/uk/support/faq/details/KA-242749/",
     "FAQ for REDMI 13C (4G/5G not specified): 18W fast charge, 10W standard charger.", 1),
    (207, "Redmi 13", 5030, None, 33, "ATL|LIWINON|COSMX", FAQ + "KA-231260/",
     "FAQ: ATL, LWN, COS; 5030mAh typ. 33W per Wikipedia. Variant (4G/5G) of this row unconfirmed.", 1),
    (208, "Redmi 13", 5030, None, 33, "ATL|LIWINON|COSMX", FAQ + "KA-231260/",
     "FAQ: ATL, LWN, COS; 5030mAh typ. 33W per Wikipedia.", 0),
    (181, "Redmi A3", None, None, None, "ATL|LIWINON|COSMX", FAQ + "KA-243547/",
     "FAQ: single-cell battery; suppliers ATL, Liwinon, CosMX. Capacity not captured. Do NOT copy to A3x (record 5).", 0),
    (193, "Poco F7", 6500, 6330, 90, "LIWINON", "https://www.mi.com/uk/support/faq/details/KA-577311/",
     "UK FAQ: dual-cell, supplier 'LWN', 4.53V limit, 22.5W reverse. SKU 25053PC47G (global). India variant is 7550 (GSMArena).", 0),
    (194, "Redmi Turbo 4 Pro", 7550, None, 90, None,
     "https://www.gizchina.com/xiaomi/redmi-turbo-4-pro-launched-with-snapdragon-8s-gen-4",
     "7550mAh typ (several news sources; the 7660 figure was wrong). Same capacity as Poco F7 India. No supplier found.", 0),
    (194, "Poco F7", None, None, None, None, None,
     "Row not filled: this codename row may be the India 7550mAh model; region unconfirmed.", 1),
    (108, "Mi 9", None, None, None, None, None,
     "Sunwoda hint UNSUPPORTED: only source was an aftermarket BM3L seller page, no manufacturer named.", 1),
]

# CATL heuristic hints to retire (kept as text for audit, no longer presented as a hint)
CATL_RECORDS = [88, 89, 92, 93, 94, 171, 193, 194]

# ---------------------------------------------------------------- EU label (EPREL)
# (model_identifier, trademark, rated_mah, cycles, endurance, class, repair_idx, user_repl, market_date, eprel_id, url)
EPREL_BASE = "https://eprel.ec.europa.eu/screen/product/smartphonestablets20231669/"
EPREL_FICHE = "https://eprel.ec.europa.eu/fiches/smartphonestablets20231669/Fiche_%s_EN.pdf"
EPREL = [
    ("24117RN76E", "Redmi", 5350, 1600, "48h49", None, 2.63, "Yes", None, 2247094, EPREL_BASE + "2247094"),
    ("24115RA8EG", "Redmi", 4990, 1000, "50h09", None, 2.61, "No", None, 2305579, EPREL_BASE + "2305579"),
    ("25078RA3EE", "Redmi", 5850, 1000, "77h", "A", None, "Yes", "2025-08-20", 2379211, EPREL_FICHE % "2379211"),
    ("2510EPC8BG", "POCO", 6330, 1000, "60h", "B", 2.63, "No", "2026-01-06", 2480173, EPREL_FICHE % "2480173"),
    ("2409FPCC4G", "POCO", 5010, 1000, "37h", "D", 2.61, "Yes", None, 2309768, "https://eprel.ec.europa.eu/qr/2309768"),
    ("24116RACCG", "Redmi", None, None, None, None, None, None, "2025-06-20", 2305634, EPREL_FICHE % "2305634"),
    ("25099RP13G", "Redmi (tablet)", 11800, 1000, "103h", "G", 2.74, "No", None, 2440269, EPREL_FICHE % "2440269"),
]

# ---------------------------------------------------------------- evidence ledger
# (record, device, claim_type, value, region, source_url, source_type, status, note)
EVIDENCE = [
    (55, "Mi 11", "cell_supplier", "Sunwoda", None,
     "https://brodos.com/wp-content/uploads/2021/03/Xiaomi-Mi-11-UN38.3.pdf", "UN38.3 report", "verified", "BM4X"),
    (81, "Mi 14", "battery_mah_typical", "4610", "global", FAQ + "KA-243623/", "Xiaomi FAQ", "verified", None),
    (83, "Mi 14 Ultra", "battery_mah_typical", "5000", "global", FAQ + "KA-108264/", "Xiaomi FAQ", "verified", None),
    (83, "Mi 14 Ultra", "battery_mah_typical", "5300", "CN", "https://en.wikipedia.org/wiki/Xiaomi_14", "Wikipedia", "secondary", None),
    (86, "Mi 15", "battery_mah_typical", "5240", "global", FAQ + "KA-541914/", "Xiaomi FAQ", "verified", None),
    (86, "Mi 15", "battery_mah_typical", "5400", "CN", "https://en.wikipedia.org/wiki/Xiaomi_15", "Wikipedia", "secondary", None),
    (88, "Mi 15 Ultra", "battery_mah_typical", "5410", "global", "https://en.wikipedia.org/wiki/Xiaomi_15", "Wikipedia", "secondary", None),
    (88, "Mi 15 Ultra", "battery_mah_typical", "6000", "CN", "https://en.wikipedia.org/wiki/Xiaomi_15", "Wikipedia", "secondary", "NotebookLM value is the China figure"),
    (91, "Mi 15T Pro", "cycles_claim", ">=80% after 1600 cycles", "global", "https://www.mi.com/global/product/xiaomi-15t-pro/", "Xiaomi product page", "marketing_claim", "Xiaomi internal labs"),
    (193, "Poco F7", "battery_mah_typical", "6500", "global", "https://www.mi.com/uk/support/faq/details/KA-577311/", "Xiaomi FAQ", "verified", None),
    (193, "Poco F7", "battery_mah_typical", "7550", "IN", "https://www.gsmarena.com/poco_f7_is_official_sd_8s_gen_4_6500mah_battery_90w_charging-news-68377.php", "GSMArena", "secondary", "Same capacity as Redmi Turbo 4 Pro"),
    (193, "Poco F7", "cycles_claim", ">=80% after 1600 cycles", "global", "https://www.mi.com/global/product/poco-f7/", "Xiaomi product page", "marketing_claim", "Xiaomi internal labs"),
    (194, "Redmi Turbo 4 Pro", "battery_mah_typical", "7660", None, None, "NotebookLM", "rejected", "7550 is correct"),
    (77, "Mi 13 Pro", "cell_supplier", "ATL", None, None, "NotebookLM/TechInsights", "unverified", "TechInsights report not opened"),
    (108, "Mi 9", "cell_supplier", "Sunwoda", None, None, "aftermarket seller page", "unsupported", "No manufacturer named in the page"),
    (81, "Mi 14", "battery_mah_rated", "4610", None, None, "NotebookLM", "rejected", "4610 is typical, not rated"),
    (None, "REDMI 15C 5G", "cell_supplier", "COSMX|ATL|SUNWODA", "global", FAQ + "KA-602453/", "Xiaomi FAQ", "verified",
     "Not in base list. 6000mAh typ, 33W, 1000 cycles to >=80% (Xiaomi lab claim)."),
    (None, "REDMI 15C (4G)", "cell_supplier", None, "global", FAQ + "KA-597672/", "Xiaomi FAQ", "not_captured",
     "FAQ has a cell-supplier question but the answer did not show. Do not copy from the 5G."),
    (None, "Redmi 17 4G", "eu_label", "7500mAh, 73h54, 1000 cycles to 80%, IP64, drop B, repair C", "EU",
     "https://en.gizchina.it/2026/07/xiaomi-redmi-17-4g-etichetta-energetica-europa/", "news report of EPREL", "secondary",
     "Not in base list"),
]

ALLOWED = {"battery_mah_typical", "battery_mah_rated", "charge_wattage", "cell_supplier", "source_url", "notes", "needs_review"}

EXTRA_SCHEMA = """
CREATE TABLE suppliers(name TEXT PRIMARY KEY, aliases TEXT, supplier_group TEXT NOT NULL, status TEXT NOT NULL, note TEXT);
CREATE TABLE eprel(
  model_identifier TEXT PRIMARY KEY, trademark TEXT, rated_mah INTEGER, cycles INTEGER, endurance_per_cycle TEXT,
  energy_class TEXT, repairability_index REAL, battery_user_replaceable TEXT, market_date TEXT,
  eprel_id INTEGER, source_url TEXT, record INTEGER, device_name TEXT);
CREATE TABLE evidence(
  id INTEGER PRIMARY KEY, record INTEGER, device_name TEXT, claim_type TEXT NOT NULL, value TEXT, region TEXT,
  source_url TEXT, source_type TEXT, status TEXT NOT NULL, note TEXT, checked_on TEXT);
CREATE VIEW v_sku_verified AS
  SELECT v.sku, v.model, d.codename, s.battery_mah_typical, s.battery_mah_rated, s.charge_wattage, s.cell_supplier,
         (SELECT group_concat(DISTINCT x.supplier_group) FROM suppliers x
            WHERE instr('|'||s.cell_supplier||'|', '|'||x.name||'|')>0) AS supplier_groups,
         e.rated_mah AS eu_rated_mah, e.cycles AS eu_cycles, e.energy_class AS eu_class, s.needs_review
  FROM variants v JOIN devices d ON d.record=v.record
  LEFT JOIN specs s ON s.record=v.record AND lower(s.device_name)=lower(v.model)
  LEFT JOIN eprel e ON e.model_identifier=v.sku;
"""


def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(65536), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="src", default="Registry/xiaomi_devices.db")
    ap.add_argument("--out", default="Registry/xiaomi_devices_verified.db")
    a = ap.parse_args()

    tmp = a.out + ".tmp"
    for p in (tmp,):
        if os.path.exists(p):
            os.chmod(p, 0o644); os.remove(p)
    shutil.copyfile(a.src, tmp)
    os.chmod(tmp, 0o644)
    con = sqlite3.connect(tmp)
    con.row_factory = sqlite3.Row
    warns = []

    con.executescript(EXTRA_SCHEMA)
    con.executemany("INSERT INTO suppliers VALUES(?,?,?,?,?)", SUPPLIERS)

    # specs
    n_spec = 0
    for rec, name, typ, rated, watt, sup, url, note, rev in SPECS:
        row = con.execute("SELECT id, notes FROM specs WHERE record=? AND lower(device_name)=lower(?)", (rec, name)).fetchone()
        if not row:
            warns.append(f"no specs row for record {rec} {name!r} - skipped"); continue
        vals = {"battery_mah_typical": typ, "battery_mah_rated": rated, "charge_wattage": watt,
                "cell_supplier": sup, "source_url": url}
        sets = {k: v for k, v in vals.items() if v is not None}
        sets["notes"] = ((row["notes"] or "") + " " + note).strip()
        sets["needs_review"] = rev
        assert set(sets) <= ALLOWED
        con.execute(f"UPDATE specs SET {', '.join(k + '=?' for k in sets)} WHERE id=?", [*sets.values(), row["id"]])
        n_spec += 1

    # retire CATL heuristic hints (basis text kept for audit)
    for rec in CATL_RECORDS:
        con.execute("UPDATE devices SET cell_hint_supplier=NULL, cell_hint_confidence=NULL, "
                    "cell_hint_basis='rejected: CATL is an EV supplier; was a heuristic guess' "
                    "WHERE record=? AND cell_hint_supplier='CATL'", (rec,))

    # EPREL: link to a model through the SKU when variants has it
    n_ep = 0
    for ident, tm, rated, cyc, end, cls, idx, repl, mdate, eid, url in EPREL:
        var = con.execute("SELECT record, model FROM variants WHERE sku=? LIMIT 1", (ident,)).fetchone()
        rec, name = (var["record"], var["model"]) if var else (None, None)
        con.execute("INSERT INTO eprel VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    (ident, tm, rated, cyc, end, cls, idx, repl, mdate, eid, url, rec, name))
        n_ep += 1
        # fill rated capacity only where the specs row has none
        if var and rated:
            r = con.execute("UPDATE specs SET battery_mah_rated=?, notes=trim(coalesce(notes,'')||' EU label rated '||?||'mAh (EPREL '||?||').') "
                            "WHERE record=? AND lower(device_name)=lower(?) AND battery_mah_rated IS NULL",
                            (rated, rated, eid, rec, name))
            if r.rowcount == 0:
                pass

    con.executemany("INSERT INTO evidence(record,device_name,claim_type,value,region,source_url,source_type,status,note,checked_on) "
                    "VALUES(?,?,?,?,?,?,?,?,?,?)", [e + (TODAY,) for e in EVIDENCE])

    con.execute("INSERT OR REPLACE INTO meta VALUES('findings_applied_on',?)", (TODAY,))
    con.execute("INSERT OR REPLACE INTO meta VALUES('findings_script','apply_findings.py')")
    con.commit()
    ic = con.execute("PRAGMA integrity_check").fetchone()[0]
    fk = con.execute("PRAGMA foreign_key_check").fetchall()
    if ic != "ok" or fk:
        raise SystemExit(f"integrity failure: {ic} {fk[:3]}")
    con.execute("PRAGMA journal_mode=DELETE")
    con.execute("VACUUM")
    con.close()
    os.chmod(tmp, 0o444)
    os.replace(tmp, a.out)
    with open(a.out + ".sha256", "w") as f:
        f.write(f"{sha(a.out)}  {os.path.basename(a.out)}\n")
    print(f"OK {a.out}: {n_spec} specs updated, {n_ep} EPREL rows, {len(EVIDENCE)} evidence rows, {len(SUPPLIERS)} suppliers")
    for w in warns:
        print("WARN", w)


if __name__ == "__main__":
    main()
