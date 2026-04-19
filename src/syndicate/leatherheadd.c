#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/tmp/leatherheadd.pid"
#define LOG_PREFIX "[LEATHERHEAD]"

static void write_pid(void) {
    FILE *f = fopen(PID_FILE, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static int is_already_running(void) {
    FILE *f = fopen(PID_FILE, "r");
    if (!f) return 0;
    int pid;
    if (fscanf(f, "%d", &pid) == 1 && kill(pid, 0) == 0) {
        fclose(f);
        return 1;
    }
    fclose(f);
    unlink(PID_FILE);
    return 0;
}

static void cleanup(int sig) {
    unlink(PID_FILE);
    printf("%s Shutdown complete.\n", LOG_PREFIX);
    exit(0);
}

static float get_skin_temp(void) {
    FILE *fp = popen("/system/bin/dumpsys thermalservice | grep -A 20 'mType=3' | grep 'mName=SKIN' | grep -o 'mValue=[0-9.]*'", "r");
    if (!fp) return -1.0;

    char line[64];
    float temp = -1.0;
    if (fgets(line, sizeof(line), fp)) {
        sscanf(line, "mValue=%f", &temp);
    }
    pclose(fp);
    return temp;
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    printf("%s ONLINE — Thermal Warden (real skin temp vs fake HAL)\n", LOG_PREFIX);

    while (1) {
        float skin = get_skin_temp();

        if (skin > 0) {
            if (skin >= 45.0) {
                printf("%s ⚠️  CRITICAL HEAT — Skin %.1f°C — throttling likely\n", LOG_PREFIX, skin);
            } else if (skin >= 38.0) {
                printf("%s WARNING — Skin %.1f°C — watch for thermal throttling\n", LOG_PREFIX, skin);
            } else {
                printf("%s Nominal — Real skin temp: %.1f°C\n", LOG_PREFIX, skin);
            }
        } else {
            printf("%s ⚠️  Could not read real skin temperature\n", LOG_PREFIX);
        }

        sleep(10);   // Optimized polling
    }

    return 0;
}
