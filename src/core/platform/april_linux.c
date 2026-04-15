#include <april_platform.h>
#include <leo_detection.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

static bool g_initialized = false;

SENSEI_STATUS april_platform_init(void) {
    if (g_initialized) return SENSEI_STATUS_OK;
    g_initialized = true;
    return SENSEI_STATUS_OK;
}

void april_platform_cleanup(void) {
    g_initialized = false;
}

SENSEI_STATUS april_elevate_privileges(void) {
    return (geteuid() == 0) ? SENSEI_STATUS_OK : SENSEI_STATUS_ACCESS_DENIED;
}

// Direct RISH process enumeration
SENSEI_STATUS april_enum_processes(SENSEI_PROCESS_LIST *list) {
    if (!list) return SENSEI_STATUS_ERROR;
    list->head = list->tail = NULL;
    list->count = 0;

    char *output = rish_pipe_command("ps -A -o pid,comm");
    if (!output) {
        return SENSEI_STATUS_ERROR;
    }

    char *line = strtok(output, "\n");
    int first = 1;
    while (line) {
        if (first) { first = 0; line = strtok(NULL, "\n"); continue; }
        // Skip leading spaces
        while (*line == ' ') line++;
        char *pid_str = line;
        while (*line && !isspace(*line)) line++;
        if (!*line) break;
        *line++ = '\0';
        while (*line == ' ') line++;
        char *comm = line;

        uint32_t pid = (uint32_t)atoi(pid_str);
        if (pid == 0) { line = strtok(NULL, "\n"); continue; }

        SENSEI_PROCESS_INFO info = {0};
        info.pid = pid;
        strncpy(info.name, comm, SENSEI_MAX_PROCESS_NAME - 1);
        info.name[SENSEI_MAX_PROCESS_NAME - 1] = '\0';
        april_process_list_append(list, &info);

        line = strtok(NULL, "\n");
    }
    free(output);
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS april_get_process_info(uint32_t pid, SENSEI_PROCESS_INFO *info) {
    if (!info) return SENSEI_STATUS_ERROR;
    memset(info, 0, sizeof(*info));
    info->pid = pid;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "ps -p %u -o comm=", pid);
    char *output = rish_pipe_command(cmd);
    if (output) {
        char *nl = strchr(output, '\n');
        if (nl) *nl = '\0';
        strncpy(info->name, output, SENSEI_MAX_PROCESS_NAME - 1);
        free(output);
    }
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS april_enum_memory_regions(uint32_t pid, SENSEI_MEMORY_REGION **regions) {
    if (!regions) return SENSEI_STATUS_ERROR;
    *regions = NULL;

    char path_buf[256];
    snprintf(path_buf, sizeof(path_buf), "/proc/%u/maps", pid);
    FILE *fp = fopen(path_buf, "r");
    if (!fp) return SENSEI_STATUS_ACCESS_DENIED;

    SENSEI_MEMORY_REGION *head = NULL, *tail = NULL;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        SENSEI_MEMORY_REGION *region = calloc(1, sizeof(SENSEI_MEMORY_REGION));
        if (!region) {
            fclose(fp);
            april_memory_region_list_free(head);
            return SENSEI_STATUS_NO_MEMORY;
        }
        unsigned long long start, end;
        char perms[5] = {0};
        unsigned long long offset;
        char dev[16] = {0};
        unsigned long inode;
        char mapped_path[SENSEI_MAX_PATH] = {0};
        int fields = sscanf(line, "%llx-%llx %4s %llx %15s %lu %1023[^\n]",
                            &start, &end, perms, &offset, dev, &inode, mapped_path);
        if (fields < 6) {
            free(region);
            continue;
        }
        region->base_address = start;
        region->size = end - start;
        region->is_executable = (perms[2] == 'x');
        region->is_writable = (perms[1] == 'w');
        if (fields >= 7 && mapped_path[0] != '\0') {
            char *trimmed = mapped_path;
            while (*trimmed == ' ') trimmed++;
            strncpy(region->mapped_file, trimmed, SENSEI_MAX_PATH - 1);
        }
        region->next = NULL;
        if (!head) {
            head = region;
            tail = region;
        } else {
            tail->next = region;
            tail = region;
        }
    }
    fclose(fp);
    *regions = head;
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS april_read_process_memory(uint32_t pid, uint64_t address,
                                        void *buffer, size_t size,
                                        size_t *bytes_read) {
    if (!buffer || !bytes_read) return SENSEI_STATUS_ERROR;
    *bytes_read = 0;
    char path_buf[256];
    snprintf(path_buf, sizeof(path_buf), "/proc/%u/mem", pid);
    int fd = open(path_buf, O_RDONLY);
    if (fd < 0) return SENSEI_STATUS_ACCESS_DENIED;
    ssize_t n = pread(fd, buffer, size, (off_t)address);
    close(fd);
    if (n < 0) return SENSEI_STATUS_ERROR;
    *bytes_read = (size_t)n;
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS april_check_syscall_table(SENSEI_DETECTION_LIST *detections) {
    (void)detections;
    return SENSEI_STATUS_UNSUPPORTED;
}

SENSEI_STATUS april_check_idt(SENSEI_DETECTION_LIST *detections) {
    (void)detections;
    return SENSEI_STATUS_UNSUPPORTED;
}

uint64_t april_get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

uint32_t april_get_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (n > 0) ? (uint32_t)n : 1;
}

uint64_t april_get_total_memory(void) {
    struct sysinfo si;
    if (sysinfo(&si) == 0)
        return (uint64_t)si.totalram * si.mem_unit;
    return 0;
}
