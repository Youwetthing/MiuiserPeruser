#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define SOCK_PATH "/data/data/com.termux/files/home/tmp/turtlecom.sock"
#define BUF_SIZE 4096
#define MAX_CORES 64

typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} cpu_stat_t;

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

static int detect_cores(void) {
    int count = 0;
    char path[128];

    for (int i = 0; i < MAX_CORES; i++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d", i);
        if (access(path, F_OK) == 0)
            count++;
        else
            break;
    }

    if (count == 0) count = 1;
    return count;
}

static int read_file(const char *path, char *buf, size_t buflen) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    if (!fgets(buf, buflen, f)) {
        fclose(f);
        return -1;
    }

    buf[strcspn(buf, "\n")] = 0;
    fclose(f);
    return 0;
}

static int cpu_online(int core) {
    char path[128], buf[32];

    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", core);
    if (read_file(path, buf, sizeof(buf)) == 0) {
        return (strcmp(buf, "1") == 0);
    }

    /* Many kernels omit cpu0/online; assume online if file missing */
    return 1;
}

static long cpu_freq_khz(int core) {
    char path[160], buf[64];

    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core);
    if (read_file(path, buf, sizeof(buf)) == 0) {
        return strtol(buf, NULL, 10);
    }

    return -1;
}

static void cpu_governor(int core, char *out, size_t outlen) {
    char path[160];

    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", core);
    if (read_file(path, out, outlen) != 0) {
        snprintf(out, outlen, "unknown");
    }
}

static int read_total_cpu_stat(cpu_stat_t *st) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }

    /* line starts with "cpu " */
    if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
               &st->user, &st->nice, &st->system, &st->idle,
               &st->iowait, &st->irq, &st->softirq, &st->steal) < 4) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

static double cpu_load_percent(const cpu_stat_t *prev, const cpu_stat_t *cur) {
    unsigned long long prev_idle = prev->idle + prev->iowait;
    unsigned long long idle = cur->idle + cur->iowait;

    unsigned long long prev_non_idle =
        prev->user + prev->nice + prev->system +
        prev->irq + prev->softirq + prev->steal;
    unsigned long long non_idle =
        cur->user + cur->nice + cur->system +
        cur->irq + cur->softirq + cur->steal;

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long total = idle + non_idle;

    unsigned long long totald = total - prev_total;
    unsigned long long idled = idle - prev_idle;

    if (totald == 0) return 0.0;

    double usage = (double)(totald - idled) * 100.0 / (double)totald;
    if (usage < 0.0) usage = 0.0;
    if (usage > 100.0) usage = 100.0;
    return usage;
}

int main(void) {
    int fd = connect_to_turtlecom();
    char out[BUF_SIZE];

    int cores = detect_cores();
    printf("rocksteadyd (Rocksteady CPU daemon): ONLINE with %d cores\n", cores);

    write(fd, "HELLO WORKER ROCKSTEADY\n", 25);

    cpu_stat_t prev = {0}, cur = {0};
    int have_prev = 0;

    for (;;) {
        if (read_total_cpu_stat(&cur) != 0) {
            /* If /proc/stat fails, just sleep and retry */
            sleep(10);
            continue;
        }

        double load = 0.0;
        if (have_prev) {
            load = cpu_load_percent(&prev, &cur);
        } else {
            have_prev = 1;
        }
        prev = cur;

        long total_freq = 0;
        int online_cores = 0;

        char per_core_buf[1024];
        per_core_buf[0] = '\0';

        for (int i = 0; i < cores; i++) {
            int online = cpu_online(i);
            long freq = cpu_freq_khz(i);
            char gov[32];
            cpu_governor(i, gov, sizeof(gov));

            if (online) {
                online_cores++;
                if (freq > 0) total_freq += freq;
            }

            char tmp[128];
            snprintf(tmp, sizeof(tmp),
                     "C%d_ON=%d C%d_FREQ=%ld C%d_GOV=%s ",
                     i, online ? 1 : 0,
                     i, freq,
                     i, gov);

            if (strlen(per_core_buf) + strlen(tmp) < sizeof(per_core_buf)) {
                strcat(per_core_buf, tmp);
            }
        }

        long avg_freq = 0;
        if (online_cores > 0 && total_freq > 0) {
            avg_freq = total_freq / online_cores;
        }

        snprintf(out, sizeof(out),
                 "STATUS ROCKSTEADY "
                 "CORES=%d ONLINE_CORES=%d "
                 "CPU_LOAD=%.1f "
                 "AVG_FREQ_KHZ=%ld %s\n",
                 cores,
                 online_cores,
                 load,
                 avg_freq,
                 per_core_buf);

        write(fd, out, strlen(out));

        sleep(10);
    }

    return 0;
}
