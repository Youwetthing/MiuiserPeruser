#include "compat/sensei_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int raph_network_scan() {
    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) return 0;

    char line[256];
    int connections = 0;
    while (fgets(line, sizeof(line), fp)) {
        // Look for '01' status which means ESTABLISHED
        if (strstr(line, " 01 ")) {
            connections++;
        }
    }
    fclose(fp);
    return connections; 
}
