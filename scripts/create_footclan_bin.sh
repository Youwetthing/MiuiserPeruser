#!/data/data/com.termux/files/usr/bin/bash
# Create a clean bin/ directory with all Foot Clan daemons

PROJECT_DIR="$HOME/MiuiserPeruser"
BIN_DIR="$PROJECT_DIR/bin"

mkdir -p "$BIN_DIR"

echo "Creating clean Foot Clan bin/ directory..."

# Copy or symlink root-level foot_* 
for f in foot_*; do
    if [ -x "$f" ]; then
        cp -f "$f" "$BIN_DIR/" 2>/dev/null || ln -sf "$PWD/$f" "$BIN_DIR/$f"
        echo "Added $f"
    fi
done

# Copy important ones from build/
for d in build/src/daemon/*d; do
    if [ -x "$d" ]; then
        name=$(basename "$d")
        cp -f "$d" "$BIN_DIR/" 2>/dev/null || ln -sf "$PWD/$d" "$BIN_DIR/$name"
        echo "Added $name from build/"
    fi
done

echo ""
echo "✅ Bin directory ready:"
ls -l "$BIN_DIR/" | head -20
echo ""
echo "You can now update start_footclan.sh to use $BIN_DIR"
