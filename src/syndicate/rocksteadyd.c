#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq"); }
int main(void) {
    MonitorConfig cfg = { .name="rocksteadyd", .base_sleep=8, .observe=observe, .log_tag="CPU_SCAN" };
    monitor_run(&cfg); return 0;
}
