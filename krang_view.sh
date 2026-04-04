#!/data/data/com.termux/files/usr/bin/bash

# Krang's Command Center
# Auto-refreshing Syndicate Dashboard

while true; do
    clear
    echo -e "\e[1;35m--- KRANG'S COMMAND CENTER (SYNDICATE LIVE) ---\e[0m"
    echo -e "\e[1;32m[TIME]\e[0m $(date +%H:%M:%S)"
    echo "-----------------------------------------------"
    
    # Poll the Generals via Splinter
    echo -e "\e[1;36mCPU   :\e[0m $(echo "FREQ" | nc -U ~/MiuiserPeruser/pipes/turtlecom.sock)"
    echo -e "\e[1;36mDISK  :\e[0m $(echo "DISK" | nc -U ~/MiuiserPeruser/pipes/turtlecom.sock)"
    echo -e "\e[1;36mTEMP  :\e[0m $(echo "TEMP" | nc -U ~/MiuiserPeruser/pipes/turtlecom.sock)"
    echo -e "\e[1;36mPROC  :\e[0m $(echo "PROCS" | nc -U ~/MiuiserPeruser/pipes/turtlecom.sock)"
    echo -e "\e[1;36mMIUI  :\e[0m $(echo "MIUI" | nc -U ~/MiuiserPeruser/pipes/turtlecom.sock)"
    
    echo "-----------------------------------------------"
    echo -e "\e[1;33m[DEBUG]\e[0m Checking Sewer Pipes..."
    ls ~/MiuiserPeruser/pipes/*.sock | wc -l | xargs echo "Active Pipes:"
    
    sleep 2
done
