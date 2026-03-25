#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 2048

static int connect_to_turtlecom(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCK_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        exit(1);
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static char* run_cmd(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;

    char *buf = malloc(1024);
    if (!buf) { pclose(f); return NULL; }

    if (!fgets(buf, 1024, f)) {
        free(buf);
        pclose(f);
        return NULL;
    }

    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

static int count_lines(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return -1;

    int c, lines = 0;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') lines++;
    }
    pclose(f);
    return lines;
}

int main(void) {
    int fd = connect_to_turtlecom();
    char out[BUF_SIZE];

    printf("bebopd (Bebop Wakelock Daemon): ONLINE\n");
    write(fd, "HELLO WORKER BEBOP\n", 20);

    for (;;) {
        /* Try to get a “top” wakelock-ish line from batterystats */
        char *top = run_cmd(
            "dumpsys batterystats 2>/dev/null | grep -i \"wake lock\" | head -n 1"
        );

        /* Rough count of wakelock-related lines */
        int total = count_lines(
            "dumpsys batterystats 2>/dev/null | grep -i \"wake lock\""
        );

        if (total < 0) total = 0;

        snprintf(out, sizeof(out),
                 "STATUS BEBOP "
                 "WAKELOCK_LINES=%d "
                 "TOP=\"%s\"\n",
                 total,
                 top ? top : "unavailable");

        write(fd, out, strlen(out));

        free(top);
        sleep(60);
    }

    return 0;
}
