/*
 * MiuiserPeruser – Network monitor (Raphael)
 */

#include <leo_detection.h>
#include <april_platform.h>
#include <sensei_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

#define MAX_LINE 512
#define MAX_SOCKETS 1024

/* Local logging (calls April) */
extern void april_log(const char* level, const char* format, ...);

typedef struct {
    unsigned long inode;
    char local_addr[64];
    char remote_addr[64];
    int local_port;
    int remote_port;
    int uid;
    int state;
    char protocol[4];
} socket_info_t;

static void parse_hex_ip_port(const char *hex, char *addr, int *port) {
    unsigned int ip, port_hex;
    sscanf(hex, "%x:%x", &ip, &port_hex);
    *port = port_hex;
    struct in_addr in;
    in.s_addr = ip;
    strcpy(addr, inet_ntoa(in));
}

static int is_private_ip(const char *ip) {
    if (strncmp(ip, "127.", 4) == 0) return 1;
    if (strncmp(ip, "10.", 3) == 0) return 1;
    if (strncmp(ip, "192.168.", 8) == 0) return 1;
    if (strncmp(ip, "172.", 4) == 0) {
        int o2 = atoi(ip + 4);
        if (o2 >= 16 && o2 <= 31) return 1;
    }
    return 0;
}

static char* get_package_name(int uid) {
    static char pkg[256] = "?";
    FILE *fp = fopen("/data/system/packages.list", "r");
    if (!fp) return pkg;
    char line[512];
    int file_uid;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%255s %d", pkg, &file_uid) == 2 && file_uid == uid) {
            fclose(fp);
            return pkg;
        }
    }
    fclose(fp);
    snprintf(pkg, sizeof(pkg), "? (uid %d)", uid);
    return pkg;
}

static int pid_for_inode(unsigned long inode) {
    DIR *proc = opendir("/proc");
    if (!proc) return -1;
    struct dirent *entry;
    while ((entry = readdir(proc))) {
        if (!isdigit(entry->d_name[0])) continue;
        char fd_path[256];
        snprintf(fd_path, sizeof(fd_path), "/proc/%s/fd", entry->d_name);
        DIR *fd = opendir(fd_path);
        if (!fd) continue;
        struct dirent *fd_entry;
        while ((fd_entry = readdir(fd))) {
            char link[256], target[256];
            snprintf(link, sizeof(link), "/proc/%s/fd/%s", entry->d_name, fd_entry->d_name);
            ssize_t len = readlink(link, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                unsigned long sock_inode;
                if (sscanf(target, "socket:[%lu]", &sock_inode) == 1 && sock_inode == inode) {
                    closedir(fd);
                    closedir(proc);
                    return atoi(entry->d_name);
                }
            }
        }
        closedir(fd);
    }
    closedir(proc);
    return -1;
}

static void process_net_file(const char *proc_file, const char *proto,
                             socket_info_t *sockets, int *count) {
    FILE *fp = fopen(proc_file, "r");
    if (!fp) return;

    char line[MAX_LINE];
    fgets(line, sizeof(line), fp); // skip header

    while (fgets(line, sizeof(line), fp) && *count < MAX_SOCKETS - 1) {
        unsigned long inode;
        int local_port, remote_port;
        char local_hex[32], remote_hex[32];
        int state, uid;
        if (sscanf(line, "%*d: %s %s %x %*x:%*x %*x:%*x %*x %d %*d %lu",
                   local_hex, remote_hex, &state, &uid, &inode) != 5)
            continue;

        socket_info_t *s = &sockets[*count];
        s->inode = inode;
        s->state = state;
        s->uid = uid;
        strcpy(s->protocol, proto);
        parse_hex_ip_port(local_hex, s->local_addr, &s->local_port);
        parse_hex_ip_port(remote_hex, s->remote_addr, &s->remote_port);
        (*count)++;
    }
    fclose(fp);
}

SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results) {
    (void)results;
    socket_info_t sockets[MAX_SOCKETS];
    int count = 0;

    process_net_file("/proc/net/tcp", "TCP", sockets, &count);
    process_net_file("/proc/net/udp", "UDP", sockets, &count);

    for (int i = 0; i < count; i++) {
        socket_info_t *s = &sockets[i];
        if (strcmp(s->protocol, "TCP") == 0) {
            if (s->state != 1) continue; // only established
        } else { // UDP
            if (strcmp(s->remote_addr, "0.0.0.0") == 0) continue;
        }
        if (is_private_ip(s->remote_addr)) continue;

        int pid = pid_for_inode(s->inode);
        if (pid < 0) continue;

        char *ident = "?";
        if (s->uid > 0) {
            ident = get_package_name(s->uid);
        } else {
            char path[256];
            snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
            FILE *cmd = fopen(path, "r");
            if (cmd) {
                static char proc_name[256];
                if (fgets(proc_name, sizeof(proc_name), cmd)) {
                    char *p = proc_name;
                    while (*p) p++;
                    *p = '\0';
                    ident = proc_name;
                }
                fclose(cmd);
            }
        }
        april_log("NETWORK", "%s %s | PID %d (%s) -> %s:%d",
                  s->protocol,
                  (strcmp(s->protocol, "TCP") == 0 ? "connection" : "packet"),
                  pid, ident, s->remote_addr, s->remote_port);
    }
    return SENSEI_STATUS_OK;
}
