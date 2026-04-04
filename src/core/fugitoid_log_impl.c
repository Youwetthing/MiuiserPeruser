#include <stdio.h>
#include <string.h>
#include <time.h>

void fugitoid_log(const char *message) {
    FILE *f = fopen("/data/local/tmp/miuiserperuser.log", "a");
    if (f) {
        time_t now = time(NULL);
        char *date = ctime(&now);
        if (date) { date[strlen(date) - 1] = '\0'; }
        fprintf(f, "[%s] %s\n", date ? date : "UNKNOWN", message);
        fclose(f);
    }
}
