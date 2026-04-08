#include "fugitoid_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BASE "/data/data/com.termux/files/home/MiuiserPeruser"

int is_alive(const char* name) {
    char cmd[256];
    // Checks if the specific daemon path is currently in the process tree
    sprintf(cmd, "pgrep -f %s/%s > /dev/null", BASE, name);
    return system(cmd) == 0;
}

void spawn(const char* name) {
    if (fork() == 0) {
        char path[512];
        sprintf(path, "%s/%s", BASE, name);
        char *args[] = {path, NULL};
        execv(path, args);
        exit(1); // Should never reach here if execv works
    }
}

int main() {
    fugitoid_init();
    // List of every daemon we want the Resurrector to keep alive
    const char* fleet[] = {"foot_heartbeatd", "foot_portwatchd", "foot_ipcshadowd"};
    
    while(1) {
        for(int i = 0; i < 3; i++) {
            if(!is_alive(fleet[i])) {
                fugitoid_log("WARN", "Resurrecting lost limb: %s", fleet[i]);
                spawn(fleet[i]);
            }
        }
        sleep(10);
    }
    return 0;
}