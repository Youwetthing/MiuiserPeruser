#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("dumpsys power | grep -i wake"); }
int main(void) {
    MonitorConfig cfg = { .name="bebopd", .base_sleep=15, .observe=observe, .log_tag="WAKE_SCAN" };
    monitor_run(&cfg); return 0;
}
