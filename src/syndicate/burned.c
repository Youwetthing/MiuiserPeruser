#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#define PID_FILE "/data/data/com.termux/files/home/tmp/burned.pid"
#define LOG_PREFIX "[BURNED]"

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

int main(void) {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (is_already_running()) {
        printf("%s Already running — exiting.\n", LOG_PREFIX);
        return 0;
    }

    write_pid();

    printf("%s ONLINE — MIUI/HyperOS Policy & Optimization Guardian\n", LOG_PREFIX);

    while (1) {
        char *miui_opt     = run_cmd("getprop persist.sys.miui_optimization");
        char *bg_data      = run_cmd("getprop persist.sys.background_data");
        char *powerkeeper  = run_cmd("getprop persist.sys.powerkeeper");
        char *autostart    = run_cmd("getprop persist.sys.autostart");
        char *restricted   = run_cmd("getprop persist.sys.miui_restricted_mode");
        char *ram_ext      = run_cmd("getprop persist.sys.memory_extension_enabled");
        char *perf_mode    = run_cmd("getprop persist.sys.performance_mode || getprop ro.miui.performance_mode");
        char *game_turbo   = run_cmd("getprop persist.sys.game_turbo_enabled");
        char *cleaner      = run_cmd("getprop persist.sys.cleaner_level");

        printf("%s MIUI_Opt=%s | BG_Data=%s | Powerkeeper=%s | Autostart=%s | Restricted=%s\n",
               LOG_PREFIX, miui_opt, bg_data, powerkeeper, autostart, restricted);

        printf("%s RAM_Extension=%s | Performance_Mode=%s | Game_Turbo=%s | Cleaner=%s\n",
               LOG_PREFIX, ram_ext, perf_mode, game_turbo, cleaner);

        /* Background restriction status */
        if (strcmp(restricted, "1") == 0) {
            printf("%s 🔒 Background restriction is ACTIVE\n", LOG_PREFIX);
        } else {
            printf("%s Background restriction is OFF\n", LOG_PREFIX);
        }

        /* RAM extension status */
        if (strcmp(ram_ext, "1") == 0) {
            printf("%s 🧠 RAM Extension is ENABLED\n", LOG_PREFIX);
        }

        free(miui_opt); free(bg_data); free(powerkeeper);
        free(autostart); free(restricted); free(ram_ext);
        free(perf_mode); free(game_turbo); free(cleaner);

        printf("%s ❤️  Burne Thompson heartbeat — MIUI/HyperOS policy monitoring\n", LOG_PREFIX);

        sleep(15);
    }

    return 0;
}
