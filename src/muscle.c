#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void nuke(char *pkg) {
    char cmd[256];
    // Force-stop removes the app from memory immediately
    snprintf(cmd, sizeof(cmd), "am force-stop %s > /dev/null 2>&1", pkg);
    system(cmd);
}

int main() {
    printf("[!] Granite Muscle active. High-frequency suppression running.\n");
    while (1) {
        FILE *file = fopen("targets.txt", "r");
        if (file) {
            char line[128];
            while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;
                if (strlen(line) > 5) nuke(line);
            }
            fclose(file);
        }
        usleep(100000); // 0.1 second cycle - Total Dominion
    }
    return 0;
}
