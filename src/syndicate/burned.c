#include "../core/monitor_template.h"
static char *observe(void) { return backend_exec("getprop persist.sys.miui_optimization"); }
int main(void) {
    MonitorConfig cfg = { .name="burned", .base_sleep=15, .observe=observe, .log_tag="MIUI" };
    monitor_run(&cfg); return 0;
}
