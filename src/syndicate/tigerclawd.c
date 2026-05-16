#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/MiuiserPeruser/pipes/pids/tigerclawd.pid"
#define LOG_PREFIX "[TIGER CLAW]"

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

static char* read_file(const char* path) {
    FILE *f = fopen(path, "r");
    if (!f) return strdup("N/A");
    char buf[128] = {0};
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

    printf("%s ONLINE — Monitoring flash wear & I/O pressure\n", LOG_PREFIX);

    while (1) {
        char *wear = read_file("/sys/block/mmcblk0/stat");
        char *io = read_file("/proc/diskstats");

        printf("%s Flash wear: %s | I/O pressure: %s\n", LOG_PREFIX, wear, io);

        /* Simple I/O pressure warning */
        if (strstr(io, "high") || atoi(io) > 50000) {
            printf("%s ⚠️  High I/O pressure detected — possible flash wear\n", LOG_PREFIX);
        }

        free(wear);
        free(io);

        sleep(12);   // Optimized polling
    }

    return 0;
}
