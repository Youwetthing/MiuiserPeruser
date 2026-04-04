#include "fugitoid_log.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

void emit_resource_snapshot(const char *component, const char *correlation_id) {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    long mem_kb = ru.ru_maxrss;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"mem_kb\":%ld,\"utime_sec\":%ld}", mem_kb, (long)ru.ru_utime.tv_sec);
    fugitoid_log_json("INFO","resource", component, "resource_snapshot", correlation_id ? correlation_id : "", "periodic snapshot", meta);
}
