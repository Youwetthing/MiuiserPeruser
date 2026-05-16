#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/MiuiserPeruser/pipes/pids/bebopd.pid"
#define LOG_PREFIX "[BEBOP]"

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

    printf("%s ONLINE — Wake locks, alarms & heartbeat reporting\n", LOG_PREFIX);

    while (1) {
        char *wakelocks = run_cmd("dumpsys power | grep -E 'Wake Locks|Wakelock' | head -3");
        char *alarms    = run_cmd("dumpsys alarm | grep -E 'Alarm|Pending' | head -2");

        printf("%s Wake Locks: %s\n", LOG_PREFIX, wakelocks);
        printf("%s Alarms: %s\n", LOG_PREFIX, alarms);

        free(wakelocks);
        free(alarms);

        printf("%s ❤️  Heartbeat — Bebop reporting for duty\n", LOG_PREFIX);

        sleep(12);   // Optimized polling
    }

    return 0;
}
