#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/MiuiserPeruser/pipes/pids/granitord.pid"
#define LOG_PREFIX "[GRANITOR]"

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
    char buf[128] = {0};
    fgets(buf, sizeof(buf), f);
    pclose(f);
    buf[strcspn(buf, "\n")] = 0;
    return strdup(buf);
}

static int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    printf("%s ONLINE — Security / verified boot / root checker\n", LOG_PREFIX);

    while (1) {
        char *selinux   = run_cmd("getenforce 2>/dev/null");
        char *vb_state  = run_cmd("getprop ro.boot.verifiedbootstate 2>/dev/null");
        char *vb_mode   = run_cmd("getprop ro.boot.veritymode 2>/dev/null");
        char *flash_locked = run_cmd("getprop ro.boot.flash.locked 2>/dev/null");
        char *ro_secure = run_cmd("getprop ro.secure 2>/dev/null");
        char *ro_debug  = run_cmd("getprop ro.debuggable 2>/dev/null");
        char *encrypt   = run_cmd("getprop ro.crypto.state 2>/dev/null");

        int has_root = file_exists("/system/bin/su") ||
                       file_exists("/system/xbin/su") ||
                       file_exists("/sbin/su");

        printf("%s SELinux=%s | VerifiedBoot=%s | VerityMode=%s | FlashLocked=%s | Secure=%s | Debuggable=%s | Encryption=%s | Root=%s\n",
               LOG_PREFIX,
               selinux, vb_state, vb_mode, flash_locked,
               ro_secure, ro_debug, encrypt,
               has_root ? "YES" : "no");

        free(selinux); free(vb_state); free(vb_mode); free(flash_locked);
        free(ro_secure); free(ro_debug); free(encrypt);

        sleep(30);   // Optimized polling
    }

    return 0;
}
