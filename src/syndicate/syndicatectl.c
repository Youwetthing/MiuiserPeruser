#include <stdio.h>
#include <string.h>

#define STATE_FILE "src/syndicate/rahzerd/state/toggles.json"

static void set_toggle(const char *key, int value) {
    printf("[SYNDICATECTL] %s = %d\n", key, value);

    FILE *f = fopen(STATE_FILE, "a");
    if (!f) return;

    fprintf(f, "%s:%d\n", key, value);
    fclose(f);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("usage: syndicatectl <module> <0|1>\n");
        return 1;
    }

    set_toggle(argv[1], atoi(argv[2]));
    return 0;
}
