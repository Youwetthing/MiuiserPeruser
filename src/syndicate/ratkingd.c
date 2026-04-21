#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("ps -eo stat | grep Z | wc -l"); }
int main(void) {
    MonitorConfig cfg = { .name="ratkingd", .base_sleep=12, .observe=observe, .log_tag="ANOMALY" };
    monitor_run(&cfg); return 0;
}
