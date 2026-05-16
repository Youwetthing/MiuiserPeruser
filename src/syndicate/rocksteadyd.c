#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/MiuiserPeruser/pipes/pids/rocksteadyd.pid"
#define LOG_PREFIX "[ROCKSTEADY]"

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

static char* read_freq(const char* path) {
    FILE *f = fopen(path, "r");
    if (!f) return strdup("N/A");
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
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

    printf("%s ONLINE — CPU throttling + frequencies + heartbeat\n", LOG_PREFIX);

    int heartbeat_counter = 0;

    while (1) {
        /* CPU monitoring */
        char *cur0 = read_freq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        char *max0 = read_freq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
        char *min0 = read_freq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq");
        char *gov  = read_freq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

        printf("%s Freq: %s kHz (min:%s max:%s) | Governor: %s\n",
               LOG_PREFIX, cur0, min0, max0, gov);

        if (atoi(cur0) < 1000000) {
            printf("%s ⚠️  Possible CPU throttling detected\n", LOG_PREFIX);
        }

        free(cur0); free(max0); free(min0); free(gov);

        /* Heartbeat */
        heartbeat_counter++;
        if (heartbeat_counter % 6 == 0) {   // every \~48 seconds
            printf("%s ❤️  Heartbeat — Rocksteady still standing guard\n", LOG_PREFIX);
        }

        sleep(8);   // Optimized polling
    }

    return 0;
}
