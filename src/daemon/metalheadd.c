#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 1024

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

int main(void) {
    int fd = connect_to_turtlecom();
    char out[BUF_SIZE];

    write(fd, "HELLO WORKER METALHEAD\n", 24);
    printf("metalheadd (Metalhead MIUI specialist): ONLINE\n");

    for (;;) {
        char *miui_opt = run_cmd("settings get global miui_optimization");
        char *power_opt = run_cmd("settings get secure power_optimization");
        char *autostart = run_cmd("settings get secure app_auto_start");
        char *hidden_api = run_cmd("settings get global hidden_api_policy");
        char *dual_apps = run_cmd("pm list users");
        char *game_turbo = run_cmd("getprop persist.sys.miui.gamemode");
        char *bg_restrict = run_cmd("dumpsys deviceidle");

        snprintf(out, sizeof(out),
            "STATUS METALHEAD "
            "MIUI_OPT=%s "
            "POWER_OPT=%s "
            "AUTOSTART=%s "
            "HIDDEN_API=%s "
            "DUAL_APPS=%s "
            "GAME_TURBO=%s "
            "BG_RESTRICT=%s\n",

            miui_opt ? miui_opt : "unknown",
            power_opt ? power_opt : "unknown",
            autostart ? autostart : "unknown",
            hidden_api ? hidden_api : "unknown",
            (dual_apps && strstr(dual_apps, "DualSpace")) ? "enabled" : "disabled",
            (game_turbo && strstr(game_turbo, "1")) ? "active" : "inactive",
            (bg_restrict && strstr(bg_restrict, "mState=ACTIVE")) ? "normal" : "aggressive"
        );

        write(fd, out, strlen(out));

        free(miui_opt);
        free(power_opt);
        free(autostart);
        free(hidden_api);
        free(dual_apps);
        free(game_turbo);
        free(bg_restrict);

        sleep(60);
    }

    return 0;
}
