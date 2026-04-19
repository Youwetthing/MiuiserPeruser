#ifndef LEO_DETECTION_H
#define LEO_DETECTION_H

#include "sensei_types.h"

SENSEI_STATUS leo_init(void);
SENSEI_STATUS leo_full_scan(void);
void leo_shutdown(void);
SENSEI_STATUS leo_scan_process(uint32_t pid, SENSEI_DETECTION_LIST *results);

#endif
