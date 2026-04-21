#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("dumpsys thermalservice | head -20"); }
int main(void) {
    MonitorConfig cfg = { .name="leatherheadd", .base_sleep=10, .observe=observe, .log_tag="THERMAL" };
    monitor_run(&cfg); return 0;
}
