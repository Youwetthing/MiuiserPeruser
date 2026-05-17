#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/tmp/ratkingd.pid"
#define LOG_PREFIX "[RAT KING]"

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
    char buf[256] = {0};
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

    printf("%s ONLINE — Zombie processes / CPU hogs hunter\n", LOG_PREFIX);

    while (1) {
        char *zombies = run_cmd("ps -ef | grep -E 'Z|defunct| <defunct>' | wc -l");
        char *cpu_hogs = run_cmd("ps -eo pid,comm,%cpu --sort=-%cpu | head -6");

        printf("%s Zombie count: %s\n", LOG_PREFIX, zombies);
        printf("%s Top CPU hogs:\n%s\n", LOG_PREFIX, cpu_hogs);

        if (atoi(zombies) > 0) {
            printf("%s ⚠️  Zombies detected — cleaning required\n", LOG_PREFIX);
        }

        free(zombies);
        free(cpu_hogs);

        printf("%s ❤️  Rat King heartbeat — listening for noise\n", LOG_PREFIX);

        sleep(12);   // Optimized polling
    }

    return 0;
}
