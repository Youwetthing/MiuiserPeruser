#!/data/data/com.termux/files/usr/bin/bash
echo "Reviving rish..."
adb connect 127.0.0.1:5555
adb -s 127.0.0.1:5555 shell /data/app/~~C2CHUVlqKeOpbtu-w9uJ0w==/moe.shizuku.privileged.api-utq6EiKfIRd6DfIgm86S0A==/lib/arm64/libshizuku.so
sleep 8
chmod 400 ~/Rish/rish_shizuku.dex
chmod +x ~/Rish/rish
export RISH_APPLICATION_ID=com.termux
~/Rish/rish -c "echo ok" && echo "rish OK" || echo "rish FAILED"
