#!/data/data/com.termux/files/usr/bin/bash
DB="data/battery_reference.db"

sqlite3 "$DB" <<SQL
DROP TABLE IF EXISTS battery_models;
CREATE TABLE battery_models (
    model TEXT PRIMARY KEY,
    supplier TEXT,
    capacity_mah INTEGER,
    cell_type TEXT,
    android_launch TEXT,
    notes TEXT
);

INSERT OR REPLACE INTO battery_models VALUES
('Redmi 5A', 'SCUD', 3000, 'Li-Poly', 'Android 7', 'Budget baseline'),
('Redmi 5', 'SCUD', 3200, 'Li-Poly', 'Android 7', ''),
('Redmi Note 5 Pro', 'Sunwoda / SCUD', 4000, 'Li-Poly', 'Android 7-8', 'Early Note series'),
('Redmi Note 7 Pro', 'Sunwoda', 4000, 'Li-Poly', 'Android 9', ''),
('Redmi Note 8 Pro', 'Sunwoda', 4500, 'Li-Poly', 'Android 9', ''),
('Redmi Note 9 Pro', 'Sunwoda', 5020, 'Li-Poly', 'Android 10', 'Large battery era begins'),
('Redmi Note 10 Pro', 'Sunwoda / Coslight', 5020, 'Li-Poly', 'Android 11', 'BM57 cell. High cycle life (800 cycles).'),
('Redmi Note 11 Pro', 'Sunwoda', 5000, 'Li-Poly', 'Android 11', ''),
('Redmi Note 12 Pro', 'Sunwoda / SCUD', 5000, 'Li-Poly', 'Android 12', ''),
('Redmi Note 13 Pro', 'Sunwoda / CATL', 5100, 'Li-Poly', 'Android 13', 'BM6A. 67W charging.'),
('Redmi Note 14 Pro', 'Sunwoda / CATL', 5000, 'Li-Poly', 'Android 14', ''),
('Redmi 15C', 'Sunwoda / SCUD', 6000, 'Li-Poly', 'Android 14/15', 'Budget king with big battery'),
('Xiaomi 13 Pro', 'ATL', 4820, 'Li-Poly', 'Android 12', '120W charging.'),
('Xiaomi 14 Ultra', 'CATL', 5000, 'Li-Poly', 'Android 14', '90W charging.'),
('Poco X3 Pro', 'Sunwoda', 5160, 'Li-Poly', 'Android 10', 'BM4X. Supports 33W.'),
('Poco F3', 'Sunwoda', 4520, 'Li-Poly', 'Android 11', 'BM4Y.'),
('Poco F5 Pro', 'Sunwoda', 5160, 'Li-Poly', 'Android 13', ''),
('Poco F7 Pro', 'CATL / BYD', 6500, 'Li-Poly', 'Android 15', 'Largest POCO battery'),
('Xiaomi Pad 7 Pro', 'CATL', 8850, 'Li-Poly', 'Android 15', 'Tablet');
-- (Full 200+ list can be appended here — use the one you already have)
SQL

echo "✅ Battery reference database created with $(sqlite3 "$DB" "SELECT COUNT(*) FROM battery_models;") models"
