#include <stdio.h>
#include <stdlib.h>

static void run_cmd(const char *cmd, const char *label) {
    printf("[RAHZERD][RADIO] %s\n", label);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        printf("[RAHZERD][RADIO] failed: %s\n", label);
        return;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        printf("  %s", buf);
    }

    pclose(fp);
}

void scan_radio() {
    run_cmd("dumpsys wifi | head -10", "WiFi");
    run_cmd("dumpsys bluetooth_manager | head -10", "Bluetooth");
    run_cmd("dumpsys connectivity | head -10", "Connectivity");
}
