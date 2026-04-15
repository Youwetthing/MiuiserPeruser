#!/data/data/com.termux/files/usr/bin/bash
cd /data/data/com.termux/files/home/MiuiserPeruser
echo "🐢 Starting full Syndicate..."
pkill -9 -f "rocksteadyd|krangd|splinterd|bebopd|leatherheadd|metalheadd|ratkingd|shredderd|tigerclawd|granitord|foot_clan_supreme|sysportd" 2>/dev/null || true
sleep 2
nohup bin/sysportd > logs/sysportd.log 2>&1 &
nohup bin/rocksteadyd > logs/rocksteadyd.log 2>&1 &
nohup bin/krangd > logs/krangd.log 2>&1 &
nohup bin/splinterd > logs/splinterd.log 2>&1 &
nohup bin/bebopd > logs/bebopd.log 2>&1 &
nohup bin/leatherheadd > logs/leatherheadd.log 2>&1 &
nohup bin/metalheadd > logs/metalheadd.log 2>&1 &
nohup bin/ratkingd > logs/ratkingd.log 2>&1 &
nohup bin/shredderd > logs/shredderd.log 2>&1 &
nohup bin/tigerclawd > logs/tigerclawd.log 2>&1 &
nohup bin/granitord > logs/granitord.log 2>&1 &
nohup bin/foot_clan_supreme > logs/foot_clan_supreme.log 2>&1 &
echo "✅ Syndicate running. Check status with: ./start_syndicate.sh status"
