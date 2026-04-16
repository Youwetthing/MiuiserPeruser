#include "daemon_core.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

void update_thermal_warden() {
    // Using absolute path for Android system binaries
    FILE *fp = popen("/system/bin/dumpsys thermalservice", "r");
    if (!fp) {
        daemon_log_info("[WARDEN] Error: Could not execute dumpsys");
        return;
    }

    char line[512];
    float skin_temp = -1.0;
    int thermal_status = 0;
    int in_hal_section = 0; 

    while (fgets(line, sizeof(line), fp)) {
        // Capture the overall system status (often cached)
        if (strstr(line, "Thermal Status:")) {
            sscanf(line, " Thermal Status: %d", &thermal_status);
        }

        // Detect when we move from 'Cached' to 'Current HAL' data
        if (strstr(line, "Current temperatures from HAL:")) {
            in_hal_section = 1;
        }

        // Only pull SKIN temperature once we are in the HAL section
        if (in_hal_section && strstr(line, "mType=3") && strstr(line, "mName=SKIN")) {
            char *val_ptr = strstr(line, "mValue=");
            if (val_ptr) {
                sscanf(val_ptr, "mValue=%f", &skin_temp);
            }
        }
    }
    pclose(fp);

    // Decision Logic
    if (skin_temp > 0) {
        if (thermal_status >= 2 && skin_temp < 35.0) {
            daemon_log_info("[WARDEN] GHOST THROW DETECTED: Status %d but Real Skin is cool (%.1fC)", thermal_status, skin_temp);
        } else if (skin_temp >= 45.0) {
            daemon_log_info("[WARDEN] CRITICAL HEAT: Real Skin at %.1fC. Watch for throttling.", skin_temp);
        } else {
            daemon_log_info("[WARDEN] Nominal - Skin: %.1fC | Sys Status: %d", skin_temp, thermal_status);
        }
    }
}

int main(void) {
    if (!daemon_core_init("leatherheadd")) return 1;
    daemon_log_info("Leatherhead: Thermal Warden v2.1 (Truth-Seeker) Active.");

    for (;;) {
        update_thermal_warden();
        // Poll every 5 seconds to stay updated without overhead
        sleep(5);
    }

    daemon_core_shutdown();
    return 0;
}
