#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
INSTALLERS="$BASE/installers"

mkdir -p "$INSTALLERS"

echo "⚖️ Judicial System Bootstrap Starting..."

# =========================
# CREATE INSTALLERS
# =========================

cat <<'EOF1' > "$INSTALLERS/install_core.sh"
#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"
PIPES="$BASE/pipes"

mkdir -p "$PIPES"

for p in superhero.pipe judgement.pipe execution.pipe escalation.pipe; do
    [ -p "$PIPES/$p" ] || mkfifo "$PIPES/$p"
done

echo "✔ Core installed (pipes ready)"
EOF1
chmod +x "$INSTALLERS/install_core.sh"


cat <<'EOF2' > "$INSTALLERS/install_daemons.sh"
#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"

chmod +x "$BASE/cre/april_o_neil.sh" 2>/dev/null
chmod +x "$BASE/turtlepower_daemon.sh" 2>/dev/null
chmod +x "$BASE/toolkit_daemon.sh" 2>/dev/null
chmod +x "$BASE/escalation_daemon.sh" 2>/dev/null

echo "✔ Daemons installed"
EOF2
chmod +x "$INSTALLERS/install_daemons.sh"


cat <<'EOF3' > "$INSTALLERS/install_ui.sh"
#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"

chmod +x "$BASE/judicial_controller.sh" 2>/dev/null

echo "✔ UI layer installed"
EOF3
chmod +x "$INSTALLERS/install_ui.sh"


cat <<'EOF4' > "$INSTALLERS/install_recovery.sh"
#!/data/data/com.termux/files/usr/bin/bash

BASE="$HOME/MiuiserPeruser"

mkdir -p "$BASE/logs"

echo "✔ Recovery layer ready"
EOF4
chmod +x "$INSTALLERS/install_recovery.sh"

# =========================
# RUN INSTALLERS IN ORDER
# =========================

echo "⚖️ Installing Core..."
bash "$INSTALLERS/install_core.sh"

echo "⚖️ Installing Daemons..."
bash "$INSTALLERS/install_daemons.sh"

echo "⚖️ Installing UI..."
bash "$INSTALLERS/install_ui.sh"

echo "⚖️ Installing Recovery..."
bash "$INSTALLERS/install_recovery.sh"

echo "⚖️ Judicial System Fully Installed"

if [ "$1" == "start" ]; then
    echo "⚖️ Court is being convened..."
    bash "$BASE/judicial_controller.sh" start
fi

