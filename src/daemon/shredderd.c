#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include "ipc_globals.h"

#define BUF_SIZE 2048

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

    printf("shredderd (Shredder Integrity Daemon): ONLINE\n");
    write(fd, "HELLO WORKER SHREDDER\n", 23);

    for (;;) {
        char *selinux = run_cmd("getenforce 2>/dev/null");
        char *vb_state = run_cmd("getprop ro.boot.verifiedbootstate 2>/dev/null");
        char *vb_mode  = run_cmd("getprop ro.boot.veritymode 2>/dev/null");
        char *flash_locked = run_cmd("getprop ro.boot.flash.locked 2>/dev/null");
        char *ro_secure = run_cmd("getprop ro.secure 2>/dev/null");
        char *ro_debuggable = run_cmd("getprop ro.debuggable 2>/dev/null");
        char *su_path = run_cmd("which su 2>/dev/null");
        int magisk_hint =
            file_exists("/sbin/.magisk") ||
            file_exists("/data/adb/magisk") ||
            file_exists("/data/adb/modules");

        snprintf(out, sizeof(out),
                 "STATUS SHREDDER "
                 "SELINUX=%s "
                 "VB_STATE=%s "
                 "VB_MODE=%s "
                 "FLASH_LOCKED=%s "
                 "SECURE=%s "
                 "DEBUGGABLE=%s "
                 "SU=%s "
                 "MAGISK_HINT=%d\n",
                 selinux ? selinux : "unknown",
                 vb_state ? vb_state : "unknown",
                 vb_mode ? vb_mode : "unknown",
                 flash_locked ? flash_locked : "unknown",
                 ro_secure ? ro_secure : "unknown",
                 ro_debuggable ? ro_debuggable : "unknown",
                 su_path && strlen(su_path) > 0 ? "present" : "none",
                 magisk_hint ? 1 : 0);

        write(fd, out, strlen(out));

        free(selinux);
        free(vb_state);
        free(vb_mode);
        free(flash_locked);
        free(ro_secure);
        free(ro_debuggable);
        free(su_path);

        sleep(60);
    }

    return 0;
}
