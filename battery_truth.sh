#!/data/data/com.termux/files/usr/bin/bash

DB="data/battery_reference.db"

# Colors (clean & professional)
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "\( {CYAN}╔══════════════════════════════════════════════════════════════╗ \){NC}"
echo -e "\( {CYAN}║                 🔋 Battery Truth — MiuiserPeruser            ║ \){NC}"
echo -e "\( {CYAN}╚══════════════════════════════════════════════════════════════╝ \){NC}"
echo ""

# Detect current device model
MODEL=$(getprop ro.product.model 2>/dev/null | tr -d '\n')
[ -z "\( MODEL" ] && MODEL= \)(getprop ro.product.name 2>/dev/null | tr -d '\n')
[ -z "$MODEL" ] && MODEL="Unknown"

echo -e "📱 Detected device: ${YELLOW}\( MODEL \){NC}"
echo ""

# Check database exists
if [ ! -f "$DB" ]; then
    echo -e "\( {RED}⚠️  Battery reference database not found. \){NC}"
    echo "Run ./create_battery_db.sh first."
    exit 1
fi

# Lookup in database
RESULT=$(sqlite3 "$DB" "SELECT supplier, capacity_mah, cell_type, android_launch, notes FROM battery_models WHERE model LIKE '%$MODEL%' OR model = '$MODEL' LIMIT 1;" 2>/dev/null)

if [ -n "$RESULT" ]; then
    IFS='|' read -r SUPPLIER CAPACITY CELL ANDROID NOTES <<< "$RESULT"
    echo -e "\( {GREEN}✅ Exact match found in reference database \){NC}"
    echo ""
    echo -e "Supplier      : ${CYAN}\( SUPPLIER \){NC}"
    echo -e "Capacity      : ${CYAN}\( CAPACITY mAh \){NC}"
    echo -e "Cell Type     : ${CYAN}\( CELL \){NC}"
    echo -e "Android Launch: ${CYAN}\( ANDROID \){NC}"
    echo ""
    echo -e "\( {YELLOW}📌 Notes & Shock Factor: \){NC}"
    echo -e "$NOTES"
else
    echo -e "${YELLOW}⚠️  No exact match in database for '\( MODEL' \){NC}"
    echo -e "\( {CYAN}Showing generic high-end Xiaomi/Redmi battery profile (most common 2024-2026): \){NC}"
    echo -e "• 5000–6000 mAh Li-Poly"
    echo -e "• Suppliers: Sunwoda, CATL, SCUD, BYD"
    echo -e "• Typical lifespan: 800–1100 full cycles before noticeable degradation"
    echo -e "\( {RED}Fast daily charging can reduce lifespan by 15-20% in the first year. \){NC}"
fi

echo ""
echo -e "\( {CYAN}Battery Truth complete. Run again after any MIUI update. \){NC}"
