#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/tmp/metalheadd.pid"
#define LOG_PREFIX "[METALHEAD]"

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

static char* run_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return strdup("N/A");
    char buf[128] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    buf[strcspn(buf, "\n")] = 0;
    return strdup(buf);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    printf("%s ONLINE — Sensor scanning (accelerometer, gyroscope, proximity, etc.)\n", LOG_PREFIX);

    while (1) {
        char *accel = run_cmd("dumpsys sensorservice | grep -E 'accel|Accelerometer' | head -1");
        char *gyro  = run_cmd("dumpsys sensorservice | grep -E 'gyro|Gyroscope' | head -1");
        char *prox  = run_cmd("dumpsys sensorservice | grep -E 'prox|Proximity' | head -1");

        printf("%s Sensors: Accel=%s | Gyro=%s | Prox=%s\n", LOG_PREFIX, accel, gyro, prox);

        free(accel);
        free(gyro);
        free(prox);

        sleep(10);   // Optimized polling
    }

    return 0;
}
