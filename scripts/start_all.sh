#!/data/data/com.termux/files/usr/bin/bash
cd /data/data/com.termux/files/home/MiuiserPeruser
echo "Starting all daemons + Daemon Hunter..."

pkill -f "rocksteadyd|bebopd|leatherheadd|metalheadd|ratkingd|shredderd|granitord|rahzerd|daemonhunter" 2>/dev/null || true
sleep 1

nohup bin/rocksteadyd > logs/rocksteadyd.log 2>&1 &
nohup bin/bebopd > logs/bebopd.log 2>&1 &
nohup bin/leatherheadd > logs/leatherheadd.log 2>&1 &
nohup bin/metalheadd > logs/metalheadd.log 2>&1 &
nohup bin/ratkingd > logs/ratkingd.log 2>&1 &
nohup bin/shredderd > logs/shredderd.log 2>&1 &
nohup bin/granitord > logs/granitord.log 2>&1 &
nohup bin/rahzerd > logs/rahzerd.log 2>&1 &
nohup bin/daemonhunter.sh > logs/daemonhunter.log 2>&1 &

echo "All daemons + Daemon Hunter started."
ps aux | grep -E 'rocksteadyd|bebopd|leatherheadd|metalheadd|ratkingd|shredderd|granitord|rahzerd|daemonhunter' | grep -v grep
