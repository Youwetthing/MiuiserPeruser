#!/data/data/com.termux/files/usr/bin/bash
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/../env.sh"

echo "🔧 Setting up Shizuku rish for MiuiserPeruser..."

# Check if rish already exists
if [[ -f "$HOME/.shizuku/rish" ]]; then
    echo "✅ rish already installed at ~/.shizuku/rish"
else
    echo "❓ Where is your rish file located?"
    echo "   (You should have exported it from the Shizuku app)"
    read -p "Path to rish: " rish_path
    if [[ -f "$rish_path" ]]; then
        mkdir -p ~/.shizuku
        cp "$rish_path" ~/.shizuku/rish
        chmod +x ~/.shizuku/rish
        echo "✅ rish installed to ~/.shizuku/rish"
    else
        echo "⚠️ rish not found at '$rish_path'."
        echo "   Please export rish from Shizuku first, then run this script again."
        exit 1
    fi
fi

# Add to PATH if not already present
if ! grep -q '.shizuku' ~/.bashrc 2>/dev/null; then
    echo 'export PATH="$PATH:$HOME/.shizuku"' >> ~/.bashrc
    echo "✅ Added ~/.shizuku to PATH in ~/.bashrc"
    echo "   Run 'source ~/.bashrc' or restart Termux to apply."
else
    echo "✅ PATH already includes ~/.shizuku"
fi

# Test connection
echo ""
echo "📡 Testing Shizuku connection..."
if ~/.shizuku/rish -c "getprop ro.product.model" 2>/dev/null | grep -q .; then
    echo "✅ Shizuku is ready!"
else
    echo "❌ Shizuku not running or not authorized."
    echo "   Open the Shizuku app, start it, and ensure Termux is authorized."
fi

echo ""
echo "🎉 Setup complete. You can now run MiuiserPeruser tools."
