#!/data/data/com.termux/files/usr/bin/bash
BASE=/data/data/com.termux/files/home/MiuiserPeruser

# Phase 1: PID list + names
RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c \
    'for p in $(ls /proc|grep -E "^[0-9]+$"); do n=$(cat /proc/$p/comm 2>/dev/null); echo $p:${n:-?}; done' \
    | grep -vE ':kworker|:kthread|:migration|:rcuo|:rcu_|:ksoftirq|:\?' \
    > $BASE/pipes/state/pid_names

# Phase 2: All maps in one rish call
RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c \
    'for p in $(ls /proc|grep -E "^[0-9]+$"); do
       maps=$(cat /proc/$p/maps 2>/dev/null)
       rwx=$(echo "$maps" | grep -cE "rwxp 00000000 00:00 0")
       inj=$(echo "$maps" | grep -icE "frida-agent|linjector|xposed")
       exd=$(echo "$maps" | grep " /data/" | grep -vcE "apk|dex|oat|art|jar")
       echo "$p:$rwx:$inj:$exd"
     done' \
    > $BASE/pipes/state/pid_maps_summary 2>/dev/null

# Phase 3: FD counts for userspace PIDs (don_behavior)
RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c \
    'for p in $(ls /proc|grep -E "^[0-9]+$"); do
       fds=$(ls /proc/$p/fd 2>/dev/null | wc -l)
       echo "$p:$fds"
     done' \
    > /data/data/com.termux/files/home/MiuiserPeruser/pipes/state/pid_fds 2>/dev/null
