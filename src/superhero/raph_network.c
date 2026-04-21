#include <leo_detection.h>
#include <sensei_types.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct { unsigned long inode; int pid; } inode_map_t;

static int build_inode_map(inode_map_t *map, int max) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(proc)) && count < max) {
        if (!isdigit(entry->d_name[0])) continue;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%s/fd", entry->d_name);
        DIR *fd_dir = opendir(path);
        if (!fd_dir) continue;
        struct dirent *fd_ent;
        while ((fd_ent = readdir(fd_dir)) && count < max) {
            char target[128];
            char link[128];
            snprintf(link, sizeof(link), "/proc/%s/fd/%s", entry->d_name, fd_ent->d_name);
            ssize_t len = readlink(link, target, sizeof(target)-1);
            if (len > 0) {
                target[len] = '\0';
                unsigned long ino;
                if (sscanf(target, "socket:[%lu]", &ino) == 1) {
                    map[count].inode = ino;
                    map[count].pid = atoi(entry->d_name);
                    count++;
                }
            }
        }
        closedir(fd_dir);
    }
    closedir(proc);
    return count;
}

SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results) {
    inode_map_t map[256]; 
    int map_size = build_inode_map(map, 256);
    (void)results; // Quiet the unused warning for now

    FILE *fp = fopen("/proc/net/tcp", "r");
    if (!fp) return SENSEI_STATUS_ERROR;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        unsigned long inode;
        if (sscanf(line, "%*d: %*x:%*x %*x:%*x %*x %*x:%*x %*x:%*x %*x %*d %*d %lu", &inode) == 1) {
            for (int i = 0; i < map_size; i++) {
                if (map[i].inode == inode) {
                    // Logic for network detection goes here
                }
            }
        }
    }
    fclose(fp);
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS raph_memory_scan(int pid, SENSEI_DETECTION_LIST *results) {
    (void)pid; (void)results;
    return SENSEI_STATUS_OK;
}
