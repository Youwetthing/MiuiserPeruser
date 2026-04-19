#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_enabled(const char *daemon) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "jq -r '.[\"%s\"] // 0' ../Registry/daemon_state.json",
        daemon
    );

    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    char buf[8];
    fgets(buf, sizeof(buf), fp);
    pclose(fp);

    return atoi(buf);
}

int main() {
    printf("[syndicate] runtime online\n\n");

    FILE *fp = popen(
        "jq -r '.syndicate[]' ../Registry/daemon_allowlist.json",
        "r"
    );

    if (!fp) {
        printf("[syndicate] failed to open registry\n");
        return 1;
    }

    char daemon[256];

    printf("[syndicate] active daemons:\n");

    while (fgets(daemon, sizeof(daemon), fp)) {

        // trim newline
        daemon[strcspn(daemon, "\n")] = 0;

        if (is_enabled(daemon)) {
            printf(" - %s\n", daemon);
        }
    }

    pclose(fp);

    printf("\n[syndicate] load complete\n");
    return 0;
}
