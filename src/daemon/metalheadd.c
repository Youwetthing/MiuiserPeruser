#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Nuanced Metalhead: Uses rish-prefixed commands only when necessary */
static char* rish_query(const char *target_node) {
    static char buffer[128];
    char cmd[256];
    // We wrap the cat in rish to slip past MIUI SELinux
    snprintf(cmd, sizeof(cmd), "rish -c 'cat %s' 2>/dev/null", target_node);
    
    FILE *f = popen(cmd, "r");
    if (!f) return "ERR";
    if (!fgets(buffer, sizeof(buffer), f)) {
        pclose(f);
        return "N/A";
    }
    buffer[strcspn(buffer, "\n")] = 0;
    pclose(f);
    return buffer;
}

int main(void) {
    printf("METALHEADD: Shizuku-Bridge Active.\n");
    for (;;) {
        // Nuanced Polling: We don't spam. We wait for the Brain or pulse every 5s.
        char *cap = rish_query("/sys/class/power_supply/battery/capacity");
        char *temp = rish_query("/sys/class/power_supply/battery/temp");
        
        printf("[METAL] CAP: %s%% | TEMP: %s\n", cap, temp);
        
        // Sleep is the only way to stay under the 33MB threshold on HyperOS
        sleep(5); 
    }
    return 0;
}
