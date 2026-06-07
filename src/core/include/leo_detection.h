#ifndef LEO_DETECTION_H
#define LEO_DETECTION_H

#include "sensei_types.h"

SENSEI_STATUS leo_init(void);
SENSEI_STATUS leo_full_scan(void);
void leo_shutdown(void);
SENSEI_STATUS leo_scan_process(uint32_t pid, SENSEI_DETECTION_LIST *results);

#endif

/* Behavioural profiling */
void leo_set_verbose(int v);
void leo_write_detection(const char *turtle, const char *type,
                         const char *description, const char *priority,
                         int confidence);
void leo_post_scan(int finding_count, int thermal, int battery,
                   int cpu_freq, const char *depth);
void leo_export_sar(void);
void leo_purge_all(void);
void april_log_verbose(const char *module, const char *check, const char *result);

