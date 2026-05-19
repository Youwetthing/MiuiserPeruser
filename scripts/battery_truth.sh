#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

DB="data/battery_reference.db"

CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                 🔋 Battery Truth — MiuiserPeruser            ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

MODEL=$(getprop ro.product.model 2>/dev/null | tr -d '\n')
[ -z "$MODEL" ] && MODEL=$(getprop ro.product.name 2>/dev/null | tr -d '\n')
[ -z "$MODEL" ] && MODEL="Unknown"

echo -e "📱 Detected device: ${YELLOW}${MODEL}${NC}"
echo ""

if [ ! -f "$DB" ]; then
    echo -e "${RED}⚠️ Battery database missing${NC}"
    exit 1
fi

RESULT=$(sqlite3 "$DB" \
"SELECT supplier, capacity_mah, cell_type, android_launch, notes \
FROM battery_models \
WHERE model = '$MODEL' \
LIMIT 1;" 2>/dev/null)

if [ -n "$RESULT" ]; then
    IFS='|' read -r SUPPLIER CAPACITY CELL ANDROID NOTES <<< "$RESULT"

    echo -e "${GREEN}✅ Exact match found${NC}"
    echo ""
    echo -e "Supplier      : ${CYAN}${SUPPLIER}${NC}"
    echo -e "Capacity      : ${CYAN}${CAPACITY} mAh${NC}"
    echo -e "Cell Type     : ${CYAN}${CELL}${NC}"
    echo -e "Android Launch: ${CYAN}${ANDROID}${NC}"
    echo ""
    echo -e "${YELLOW}📌 Notes:${NC}"
    echo -e "${NOTES}"

else
    echo -e "${YELLOW}⚠️ No exact match for '${MODEL}'${NC}"
    echo ""
    echo -e "${CYAN}Fallback profile:${NC}"
    echo "• 5000–6000 mAh Li-Poly typical"
    echo "• Suppliers: Sunwoda / CATL / BYD"
    echo "• ~800–1100 cycles lifespan"
    echo -e "${RED}Fast charging accelerates degradation over time.${NC}"
fi

echo ""
echo -e "${CYAN}Battery Truth complete.${NC}"
