#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

typedef struct {
    int pid;
    int oom;
    int uid;
    char *cmd;
} proc_t;

static int cmp_desc(const void *a, const void *b) {
    const proc_t *pa = a, *pb = b;
    return pb->oom - pa->oom;
}

int main(void) {
    DIR *d = opendir("/proc");
    if (!d) return 1;

    struct dirent *ent;
    proc_t *list = NULL;
    size_t count = 0, cap = 128;

    list = malloc(cap * sizeof(proc_t));
    if (!list) {
        closedir(d);
        return 1;
    }

    while ((ent = readdir(d))) {
        if (!isdigit((unsigned char)ent->d_name[0])) continue;

        int pid = atoi(ent->d_name);
        char path[256], buf[4096];

        // Read oom_score
        snprintf(path, sizeof(path), "/proc/%d/oom_score", pid);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        int oom;
        if (fscanf(f, "%d", &oom) != 1) {
            fclose(f);
            continue;
        }
        fclose(f);

        // Read UID from status
        snprintf(path, sizeof(path), "/proc/%d/status", pid);
        f = fopen(path, "r");
        if (!f) continue;
        int uid = -1;
        while (fgets(buf, sizeof(buf), f)) {
            if (strncmp(buf, "Uid:", 4) == 0) {
                sscanf(buf + 4, "%d", &uid);
                break;
            }
        }
        fclose(f);
        if (uid < 0) continue;

        // Read cmdline
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        f = fopen(path, "r");
        char *cmd = NULL;
        if (f) {
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (n > 0) {
                buf[n] = '\0';
                for (size_t i = 0; i < n; i++)
                    if (buf[i] == '\0') buf[i] = ' ';
                cmd = strdup(buf);
            }
        }
        if (!cmd) {
            snprintf(buf, sizeof(buf), "[%d]", pid);
            cmd = strdup(buf);
        }
        if (!cmd) continue;

        // Grow array
        if (count == cap) {
            cap *= 2;
            proc_t *tmp = realloc(list, cap * sizeof(proc_t));
            if (!tmp) {
                free(cmd);
                break;
            }
            list = tmp;
        }

        list[count].pid = pid;
        list[count].oom = oom;
        list[count].uid = uid;
        list[count].cmd = cmd;
        count++;
    }

    closedir(d);

    // Sort by OOM descending
    qsort(list, count, sizeof(proc_t), cmp_desc);

    // Output
    for (size_t i = 0; i < count; i++) {
        printf("%d %d %d %s\n",
               list[i].pid,
               list[i].oom,
               list[i].uid,
               list[i].cmd);
        free(list[i].cmd);
    }

    free(list);
    return 0;
}
