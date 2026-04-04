#include <unistd.h>
#include <stdio.h>

int backend_shizuku_detect(void) {
    const char* bridge = "/data/data/com.termux/files/home/MiuiserPeruser/java-helper/libs/shizuku-api.jar";
    if (access(bridge, F_OK) == 0) {
        return 0; /* Shizuku Route Armed */
    }
    return -1; /* Fallback to ADB/RISH */
}
