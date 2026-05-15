#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include "ipc_globals.h"

#define BUF_SIZE 1024

static int connect_to_turtlecom(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TURTLE_SOCKET, sizeof(addr.sun_path) - 1);

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

    char *buf = malloc(256);
    if (!buf) { pclose(f); return NULL; }

    if (!fgets(buf, 256, f)) {
        free(buf);
        pclose(f);
        return NULL;
    }

    buf[strcspn(buf, "\n")] = 0;
    pclose(f);
    return buf;
}

static int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

int main(void) {
    int fd = connect_to_turtlecom();
    char out[BUF_SIZE];

    write(fd, "HELLO WORKER GRANITOR\n", 23);
    printf("granitord (Granitor Security Daemon): ONLINE\n");

    for (;;) {
        char *selinux = run_cmd("getenforce 2>/dev/null");
        char *vbstate = run_cmd("getprop ro.boot.verifiedbootstate");
        char *bootlock = run_cmd("getprop ro.boot.flash.locked");
        char *debuggable = run_cmd("getprop ro.debuggable");
        char *encrypt = run_cmd("getprop ro.crypto.state");

        int has_root =
            file_exists("/system/bin/su") ||
            file_exists("/system/xbin/su") ||
            file_exists("/sbin/su");

        snprintf(out, sizeof(out),
            "STATUS GRANITOR "
            "SELINUX=%s "
            "VERIFIED=%s "
            "BOOTLOCK=%s "
            "DEBUG=%s "
            "ENCRYPT=%s "
            "ROOT=%s\n",

            selinux ? selinux : "unknown",
            vbstate ? vbstate : "unknown",
            bootlock ? bootlock : "unknown",
            debuggable ? debuggable : "unknown",
            encrypt ? encrypt : "unknown",
            has_root ? "yes" : "no"
        );

        write(fd, out, strlen(out));

        free(selinux);
        free(vbstate);
        free(bootlock);
        free(debuggable);
        free(encrypt);

        sleep(30);
    }

    return 0;
}
