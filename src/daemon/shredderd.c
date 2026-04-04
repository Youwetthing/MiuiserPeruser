#include "daemon_core.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define BUS_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 2048

static int connect_bus(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, BUS_PATH);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static char* run(const char *cmd) {
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    static char buf[256];
    if (!fgets(buf, sizeof(buf), f)) {
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
    if (!daemon_core_init("shredderd")) return 1;

    int fd = connect_bus();
    if (fd < 0) return 1;

    write(fd, "HELLO WORKER SHREDDER\n", 23);
    daemon_log_info("shredderd ONLINE");

    char buf[BUF_SIZE];

    for (;;) {
        int n = read(fd, buf, sizeof(buf)-1);
        if (n <= 0) {
            usleep(100000);
            continue;
        }
        buf[n] = 0;

        /* Capability discovery */
        if (strncmp(buf, "CAPABILITY?", 11) == 0) {
            write(fd, "CAPABILITY SECURITY STATUS\n", 28);
            write(fd, "CAPABILITY ROOT CHECK\n", 23);
            write(fd, "CAPABILITY BOOTSTATE CHECK\n", 28);
            continue;
        }

        /* SECURITY STATUS */
        if (strncmp(buf, "SECURITY STATUS", 15) == 0) {
            char *selinux = run("getenforce 2>/dev/null");
            char *vb_state = run("getprop ro.boot.verifiedbootstate 2>/dev/null");
            char *vb_mode  = run("getprop ro.boot.veritymode 2>/dev/null");
            char *flash_locked = run("getprop ro.boot.flash.locked 2>/dev/null");
            char *ro_secure = run("getprop ro.secure 2>/dev/null");
            char *ro_debuggable = run("getprop ro.debuggable 2>/dev/null");
            char *su_path = run("which su 2>/dev/null");

            int magisk_hint =
                file_exists("/sbin/.magisk") ||
                file_exists("/data/adb/magisk") ||
                file_exists("/data/adb/modules");

            dprintf(fd,
                "SECURITY STATUS SELINUX=%s VB_STATE=%s VB_MODE=%s FLASH_LOCKED=%s "
                "SECURE=%s DEBUGGABLE=%s SU=%s MAGISK_HINT=%d\n",
                selinux ? selinux : "unknown",
                vb_state ? vb_state : "unknown",
                vb_mode ? vb_mode : "unknown",
                flash_locked ? flash_locked : "unknown",
                ro_secure ? ro_secure : "unknown",
                ro_debuggable ? ro_debuggable : "unknown",
                (su_path && strlen(su_path) > 0) ? "present" : "none",
                magisk_hint ? 1 : 0
            );
            continue;
        }

        /* ROOT CHECK */
        if (strncmp(buf, "ROOT CHECK", 10) == 0) {
            char *su_path = run("which su 2>/dev/null");
            dprintf(fd, "ROOT CHECK %s\n",
                (su_path && strlen(su_path) > 0) ? "present" : "none");
            continue;
        }

        /* BOOTSTATE CHECK */
        if (strncmp(buf, "BOOTSTATE CHECK", 15) == 0) {
            char *vb_state = run("getprop ro.boot.verifiedbootstate 2>/dev/null");
            dprintf(fd, "BOOTSTATE CHECK %s\n",
                vb_state ? vb_state : "unknown");
            continue;
        }
    }

    return 0;
}
