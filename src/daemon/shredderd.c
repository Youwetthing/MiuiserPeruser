#include "fugitoid_log.h"
#include "fugitoid_log.h"
/*
 * Shredder – supervises the DEX (Foot Clan) and forwards commands.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <errno.h>

#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/miuiserperuser_shredder.sock"
#define DEX_PATH "/data/data/com.termux/files/home/MiuiserPeruser/src/daemon/rish_shizuku.dex"
#define BUFFER_SIZE 16384

static pid_t dex_pid = -1;
static int dex_stdin = -1;
static int dex_stdout = -1;

/* Launch the DEX process (Foot Soldier) */
static int start_dex(void) {
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
        perror("pipe");
        return -1;
    }

    dex_pid = fork();
    if (dex_pid < 0) {
        perror("fork");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return -1;
    }

    if (dex_pid == 0) {
        // Child – isolate from Shredder's process group
        setsid();

        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);

        setenv("MANAGER_APPLICATION_ID", "moe.shizuku.privileged.api", 1);

        execl("/system/bin/app_process", "app_process",
              "-Djava.class.path=" DEX_PATH,
              "/system/bin", "rikka.shizuku.shell.ShizukuShellLoader", NULL);
        perror("execl app_process");
        exit(1);
    }

    // Parent – Shredder
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    dex_stdin = stdin_pipe[1];
    dex_stdout = stdout_pipe[0];

    // Read and discard banner
    char banner[256];
    read(dex_stdout, banner, sizeof(banner)); // ignore

    fprintf(stderr, "Shredder: DEX started, PID %d\n", dex_pid);
    return 0;
}

/* Send a command to the DEX and read the full response */
static char* dex_command(const char* cmd) {
    if (dex_pid == -1) return strdup("ERROR: DEX not running");

    // Send command
    dprintf(dex_stdin, "%s\n", cmd);

    // Read response (until EOF – DEX closes output after each command?)
    // Simplified: read until we have a full line, but for multi‑line we'd need a marker.
    // We'll assume the DEX returns one line for now.
    char *resp = malloc(BUFFER_SIZE);
    if (!resp) return NULL;
    int pos = 0;
    while (pos < BUFFER_SIZE-1) {
        ssize_t n = read(dex_stdout, resp + pos, 1);
        if (n <= 0) break;
        if (resp[pos] == '\n') break;
        pos++;
    }
    resp[pos] = '\0';
    return resp;
}

/* Check if DEX is alive; if dead, restart it */
static void ensure_dex(void) {
    int status;
    pid_t result = waitpid(dex_pid, &status, WNOHANG);
    if (result == dex_pid) {
        fprintf(stderr, "Shredder: DEX died, restarting...\n");
        close(dex_stdin);
        close(dex_stdout);
        dex_pid = -1;
        start_dex();
    } else if (result < 0 && errno != ECHILD) {
        perror("waitpid");
    }
}

/* Handle one client connection */
static void handle_client(int client_fd) {
    char cmd[BUFFER_SIZE];
    ssize_t len = read(client_fd, cmd, sizeof(cmd)-1);
    if (len <= 0) {
        close(client_fd);
        return;
    }
    cmd[len] = '\0';

    ensure_dex(); // restart if dead

    char *result = dex_command(cmd);
    if (result) {
        write(client_fd, result, strlen(result));
        free(result);
    } else {
        const char *err = "ERROR: command failed";
        write(client_fd, err, strlen(err));
    }
    close(client_fd);
}

int main() {
    fprintf(stderr, "Shredder starting...\n");

    // Start the DEX
    if (start_dex() < 0) {
        fprintf(stderr, "Shredder: failed to start DEX\n");
        return 1;
    }

    // Create listening socket
    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path)-1);
    unlink(SOCKET_PATH);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    fprintf(stderr, "Shredder listening on %s\n", SOCKET_PATH);

    while (1) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_fd);
        // After handling, check DEX health
        ensure_dex();
    }

    close(listen_fd);
    unlink(SOCKET_PATH);
    return 0;
}
