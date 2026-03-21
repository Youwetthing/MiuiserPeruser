#include "fugitoid_log.h"
#include "fugitoid_log.h"
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static int rish_stdin = -1;
static int rish_stdout = -1;
static pid_t rish_pid = -1;

static char *read_line(int fd) {
    char *buf = malloc(4096);
    if (!buf) return NULL;
    size_t pos = 0;
    while (pos < 4095) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            free(buf);
            return NULL;
        }
        if (c == '\n') break;
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return buf;
}

int rish_pipe_start(void) {
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        perror("pipe");
        return -1;
    }

    rish_pid = fork();
    if (rish_pid < 0) {
        perror("fork");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return -1;
    }

    if (rish_pid == 0) {
        // child process – isolate from parent's process group
        setsid();   // create new session, prevents child from killing parent

        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        // Set environment variable for Shizuku
        setenv("MANAGER_APPLICATION_ID", "moe.shizuku.privileged.api", 1);

        // Launch official Shizuku DEX
        execl("/system/bin/app_process", "app_process",
              "-Djava.class.path=/data/data/com.termux/files/home/MiuiserPeruser/src/daemon/rish_shizuku.dex",
              "/system/bin", "rikka.shizuku.shell.ShizukuShellLoader", NULL);
        perror("execl app_process");
        exit(1);
    }

    // parent
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    rish_stdin = stdin_pipe[1];
    rish_stdout = stdout_pipe[0];

    // Read and discard initial banner
    char *banner = read_line(rish_stdout);
    if (banner) free(banner);

    // Give the DEX a moment to initialise
    sleep(2);

    return 0;
}

char *rish_pipe_command(const char *cmd) {
    if (rish_pid == -1) return strdup("ERROR: rish not started");
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    write(rish_stdin, buf, strlen(buf));
    return read_line(rish_stdout);
}

void rish_pipe_stop(void) {
    if (rish_pid != -1) {
        close(rish_stdin);
        close(rish_stdout);
        kill(rish_pid, SIGTERM);
        waitpid(rish_pid, NULL, 0);
        rish_pid = -1;
    }
}
