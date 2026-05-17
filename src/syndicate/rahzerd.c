#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/tmp/rahzerd.pid"
#define LOG_PREFIX "[RAHZERD]"

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
    char *buf = malloc(1024);
    if (!buf) { pclose(f); return strdup("N/A"); }
    size_t len = fread(buf, 1, 1023, f);
    buf[len] = '\0';
    pclose(f);
    return buf;
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    printf("%s ONLINE — Full Suite Connectivity Daemon (dumpsys netstats + connectivity)\n", LOG_PREFIX);

    while (1) {
        char *netstats = run_cmd("dumpsys netstats | head -30");
        char *connectivity = run_cmd("dumpsys connectivity | head -20");

        printf("%s === NetStats ===\n%s\n", LOG_PREFIX, netstats);
        printf("%s === Connectivity ===\n%s\n", LOG_PREFIX, connectivity);

        free(netstats);
        free(connectivity);

        printf("%s ❤️  Rahzerd heartbeat — full network monitoring active\n", LOG_PREFIX);

        sleep(15);   // Optimized
    }

    return 0;
}
