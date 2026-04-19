#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define LOG_PREFIX "[LAUNCHER]"

typedef struct {
    const char *name;
    const char *path;
    pid_t pid;
} daemon_t;

static daemon_t daemons[] = {
    {"bebopd",       "./bebopd"},
    {"burned",       "./burned"},
    {"granitord",    "./granitord"},
    {"ratkingd",     "./ratkingd"},
    {"rocksteady",   "./rocksteady"},
    {"leatherhead",  "./leatherheadd"},
    {"metalhead",    "./metalheadd"},
    {"shredder",     "./shredderd"},
    {"krang",        "./krangd"},
    {"rahzerd",      "./rahzerd"},
    {"splinter",     "./splinterd"},
    {"turtlecom",    "./turtlecomd"}
};

static const int DAEMON_COUNT =
    sizeof(daemons) / sizeof(daemons[0]);

static void spawn_daemon(daemon_t *d) {
    pid_t pid = fork();

    if (pid == 0) {
        execl(d->path, d->name, NULL);
        perror("exec failed");
        exit(1);
    }

    d->pid = pid;
    printf("%s Started %s (pid %d)\n", LOG_PREFIX, d->name, pid);
}

static void restart_daemon(int i) {
    printf("%s Restarting %s\n", LOG_PREFIX, daemons[i].name);
    spawn_daemon(&daemons[i]);
}

static void cleanup(int sig) {
    printf("%s Shutting down all daemons...\n", LOG_PREFIX);

    for (int i = 0; i < DAEMON_COUNT; i++) {
        if (daemons[i].pid > 0) {
            kill(daemons[i].pid, SIGTERM);
        }
    }

    exit(0);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    printf("%s Unified daemon launcher online\n", LOG_PREFIX);

    for (int i = 0; i < DAEMON_COUNT; i++) {
        spawn_daemon(&daemons[i]);
    }

    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {
            for (int i = 0; i < DAEMON_COUNT; i++) {
                if (daemons[i].pid == pid) {
                    printf("%s %s died — restarting\n",
                        LOG_PREFIX, daemons[i].name);
                    restart_daemon(i);
                }
            }
        }

        sleep(2);
    }

    return 0;
}
