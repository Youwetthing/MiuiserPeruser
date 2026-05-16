#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#define PID_FILE "/data/data/com.termux/files/home/MiuiserPeruser/pipes/pids/krangd.pid"
#define SOCK_PATH "/data/data/com.termux/files/home/MiuiserPeruser/pipes/krang.sock"
#define LOG_PREFIX "[KRANG]"

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
    unlink(SOCK_PATH);
    printf("%s Shutdown complete.\n", LOG_PREFIX);
    exit(0);
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path)-1);

    unlink(SOCK_PATH);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 10) < 0) {
        perror("listen");
        return 1;
    }

    printf("%s ONLINE — Command / IPC hub ready at %s\n", LOG_PREFIX, SOCK_PATH);

    while (1) {
        int client = accept(fd, NULL, NULL);
        if (client < 0) continue;

        char buf[1024] = {0};
        read(client, buf, sizeof(buf)-1);
        printf("%s Received command: %s\n", LOG_PREFIX, buf);

        // Echo ACK for now (expand later)
        write(client, "ACK\n", 4);
        close(client);
    }

    return 0;
}
