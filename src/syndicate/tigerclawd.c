#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("cat /proc/diskstats | head -3"); }
int main(void) {
    MonitorConfig cfg = { .name="tigerclawd", .base_sleep=12, .observe=observe, .log_tag="IO_SCAN" };
    monitor_run(&cfg); return 0;
}
