#!/usr/bin/env python3
"""
build_devices_db.py - CSVs (+ NotebookLM result CSVs) -> locked SQLite DB.

CSV is the source of truth; the DB is a derived, read-only artifact.
Rebuild after every merge. `record` numbers = row order of the base CSV
(matches the "Record N" numbers in the NotebookLM batch PDFs). Never re-sort
or insert rows into the base CSV in the middle; append only.

  python3 tools/build_devices_db.py \
      --base data/xiaomi_devices.csv --variants data/xiaomi_variants.csv \
      --results data/nlm_batch_*.csv --out Registry/xiaomi_devices.db

Readers (C/Python) should open it read-only:
  sqlite3_open_v2("file:Registry/xiaomi_devices.db?mode=ro&immutable=1", ..., SQLITE_OPEN_URI)
"""
import argparse, csv, glob, hashlib, os, re, sqlite3, sys, time

SCHEMA_VERSION = 1
SCHEMA = """
CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE devices(
  record INTEGER PRIMARY KEY,
  supplier TEXT NOT NULL DEFAULT 'Xiaomi',
  device_name TEXT NOT NULL,
  model_sku TEXT,
  codename TEXT,
  variant_count INTEGER,
  battery_mah_typical INTEGER CHECK(battery_mah_typical IS NULL OR battery_mah_typical BETWEEN 500 AND 30000),
  battery_mah_rated   INTEGER CHECK(battery_mah_rated   IS NULL OR battery_mah_rated   BETWEEN 500 AND 30000),
  charge_wattage      INTEGER CHECK(charge_wattage      IS NULL OR charge_wattage      BETWEEN 1 AND 400),
  cell_supplier TEXT,
  cell_confidence TEXT,
  cell_basis TEXT,
  source_url TEXT,
  notes TEXT,
  needs_review INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX idx_dev_codename ON devices(codename);
CREATE INDEX idx_dev_sku ON devices(model_sku);
CREATE TABLE variants(
  id INTEGER PRIMARY KEY,
  record INTEGER REFERENCES devices(record),
  model TEXT, device_full TEXT, storage TEXT, sku TEXT, codename TEXT
);
CREATE INDEX idx_var_record ON variants(record);
CREATE INDEX idx_var_sku ON variants(sku);
"""


def num(v):
    """-> (int|None, needs_review). NOT_FOUND/blank -> (None, False)."""
    v = (v or "").strip().replace(",", "")
    if not v or v.upper() == "NOT_FOUND":
        return None, False
    nums = re.findall(r"\d+(?:\.\d+)?", v)
    if "|" in v or len(nums) != 1:
        return None, True
    return int(float(nums[0])), False


def txt(v):
    v = (v or "").strip()
    return None if (not v or v.upper() == "NOT_FOUND") else v


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(65536), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", required=True)
    ap.add_argument("--variants")
    ap.add_argument("--results", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--export-csv", help="write merged devices table back to CSV")
    a = ap.parse_args()

    base = list(csv.DictReader(open(a.base, newline="", encoding="utf-8")))
    devs = {}
    for i, r in enumerate(base, 1):
        mah, rev = num(r.get("battery_mah_typical") or r.get("battery_mah"))
        devs[i] = dict(
            record=i, supplier=r.get("supplier") or "Xiaomi", device_name=r["device_name"],
            model_sku=txt(r.get("model_sku")), codename=txt(r.get("codename")),
            variant_count=int(r["variant_count"]) if (r.get("variant_count") or "").isdigit() else None,
            battery_mah_typical=mah, battery_mah_rated=None,
            charge_wattage=num(r.get("charge_wattage"))[0],
            cell_supplier=txt(r.get("cell_supplier")), cell_confidence=txt(r.get("cell_confidence")),
            cell_basis=txt(r.get("cell_basis")), source_url=None, notes=None, needs_review=int(rev))

    # ---- merge NotebookLM result CSVs (never erases a value, later files win)
    files = [f for p in a.results for f in sorted(glob.glob(p))]
    seen = set()
    warns = []
    for fn in files:
        for r in csv.DictReader(open(fn, newline="", encoding="utf-8")):
            try:
                rec = int(str(r.get("record", "")).strip())
            except ValueError:
                warns.append(f"{fn}: bad record {r.get('record')!r}"); continue
            d = devs.get(rec)
            if not d:
                warns.append(f"{fn}: record {rec} not in base"); continue
            code = (r.get("codename") or "").strip()
            code = "" if code.upper() == "NONE" else code
            dn = (r.get("device_name") or "").lower()
            if d["codename"]:
                ok = code.lower() == d["codename"].lower()
            else:
                ok = (not code) and (dn in d["device_name"].lower() or d["device_name"].lower() in dn)
            if not ok:
                warns.append(f"{fn}: record {rec} identity mismatch (got {code!r}/{r.get('device_name')!r}, "
                             f"want {d['codename']!r}/{d['device_name']!r}) - SKIPPED")
                continue
            vals = {}
            for k in ("battery_mah_typical", "battery_mah_rated", "charge_wattage"):
                n, rev = num(r.get(k))
                if rev:
                    d["needs_review"] = 1
                    d["notes"] = ((d["notes"] or "") + f" [review {k}={r.get(k)!r}]").strip()
                elif n is not None:
                    vals[k] = n
            for k in ("cell_supplier", "source_url"):
                if txt(r.get(k)):
                    vals[k] = txt(r.get(k))
            if txt(r.get("notes")):
                d["notes"] = ((d["notes"] or "") + " " + txt(r.get("notes"))).strip()
            if (fn, rec) in seen:
                # same record twice in one file (multi-model codename): keep first values, flag the rest
                d["needs_review"] = 1
                d["notes"] = ((d["notes"] or "") + f" [extra row: {r.get('device_name')} {vals}]").strip()
                continue
            seen.add((fn, rec))
            d.update(vals)

    # ---- build DB
    out = a.out
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    tmp = out + ".tmp"
    if os.path.exists(tmp):
        os.chmod(tmp, 0o644); os.remove(tmp)
    con = sqlite3.connect(tmp)
    con.executescript(SCHEMA)
    cols = list(next(iter(devs.values())).keys())
    con.executemany(f"INSERT INTO devices({','.join(cols)}) VALUES({','.join('?'*len(cols))})",
                    [[d[c] for c in cols] for d in devs.values()])
    nvar = 0
    if a.variants:
        by_code = {d["codename"]: r for r, d in devs.items() if d["codename"]}
        by_name = {d["device_name"]: r for r, d in devs.items() if not d["codename"]}
        rows = []
        for v in csv.DictReader(open(a.variants, newline="", encoding="utf-8")):
            rec = by_code.get(v["codename"]) if v["codename"] else by_name.get(v["model"])
            rows.append((rec, v["model"], v["device_full"], v["storage"], v["sku"], v["codename"] or None))
        con.executemany("INSERT INTO variants(record,model,device_full,storage,sku,codename) VALUES(?,?,?,?,?,?)", rows)
        nvar = len(rows)
    con.executemany("INSERT INTO meta VALUES(?,?)", [
        ("schema_version", str(SCHEMA_VERSION)), ("built_at_utc", time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())),
        ("base_csv_sha256", sha256(a.base)), ("result_files", ",".join(os.path.basename(f) for f in files)),
        ("device_rows", str(len(devs))), ("variant_rows", str(nvar))])
    con.execute(f"PRAGMA user_version={SCHEMA_VERSION}")
    con.commit()
    ic = con.execute("PRAGMA integrity_check").fetchone()[0]
    fk = con.execute("PRAGMA foreign_key_check").fetchall()
    if ic != "ok" or fk:
        sys.exit(f"integrity failure: {ic} {fk[:3]}")
    con.execute("PRAGMA journal_mode=DELETE")
    con.execute("VACUUM")
    con.close()

    # ---- lock: read-only perms + hash for tamper checks
    os.chmod(tmp, 0o444)
    os.replace(tmp, out)
    with open(out + ".sha256", "w") as f:
        f.write(f"{sha256(out)}  {os.path.basename(out)}\n")
    os.chmod(out + ".sha256", 0o444)

    if a.export_csv:
        with open(a.export_csv, "w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(f, fieldnames=cols); w.writeheader()
            w.writerows(devs.values())

    filled = sum(1 for d in devs.values() if d["battery_mah_typical"])
    review = sum(d["needs_review"] for d in devs.values())
    print(f"OK {out}: {len(devs)} devices, {nvar} variants, {filled} with battery mAh, {review} need review, "
          f"{len(files)} result file(s)")
    for w_ in warns:
        print("WARN", w_)


if __name__ == "__main__":
    main()
