#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define SYSPORT_SOCKET_PATH "/data/local/tmp/sysport.sock"
#define BUF_SIZE 4096

static int setup_socket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SYSPORT_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    unlink(SYSPORT_SOCKET_PATH);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 4) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}

static void send_terminator(int client_fd) {
    const char *term = "\n.\n";
    write(client_fd, term, strlen(term));
}

static void handle_read(int client_fd, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        dprintf(client_fd, "ERROR: cannot open %s: %s\n", path, strerror(errno));
        send_terminator(client_fd);
        return;
    }

    char buf[BUF_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (write(client_fd, buf, n) < 0) {
            break;
        }
    }
    fclose(f);
    send_terminator(client_fd);
}

static void handle_exec(int client_fd, const char *cmd) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        dprintf(client_fd, "ERROR: pipe failed: %s\n", strerror(errno));
        send_terminator(client_fd);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        dprintf(client_fd, "ERROR: fork failed: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        send_terminator(client_fd);
        return;
    }

    if (pid == 0) {
        // child
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/system/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    // parent
    close(pipefd[1]);
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        if (write(client_fd, buf, n) < 0) {
            break;
        }
    }
    close(pipefd[0]);
    waitpid(pid, NULL, 0);
    send_terminator(client_fd);
}

static void handle_client(int client_fd) {
    char line[BUF_SIZE];

    ssize_t n = read(client_fd, line, sizeof(line) - 1);
    if (n <= 0) return;
    line[n] = '\0';

    // strip trailing newline
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';

    if (strncmp(line, "READ ", 5) == 0) {
        const char *path = line + 5;
        handle_read(client_fd, path);
    } else if (strncmp(line, "EXEC ", 5) == 0) {
        const char *cmd = line + 5;
        handle_exec(client_fd, cmd);
    } else {
        dprintf(client_fd, "ERROR: unknown command\n");
        send_terminator(client_fd);
    }
}

int main(void) {
    int server_fd = setup_socket();
    if (server_fd < 0) {
        fprintf(stderr, "Failed to set up sysport socket\n");
        return 1;
    }

    printf("sysportd listening on %s\n", SYSPORT_SOCKET_PATH);

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // child
            close(server_fd);
            handle_client(client_fd);
            close(client_fd);
            _exit(0);
        } else {
            // parent
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
