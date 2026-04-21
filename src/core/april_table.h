#ifndef APRIL_TABLE_H
#define APRIL_TABLE_H

#include <stdint.h>

#define APRIL_BIN "/data/data/com.termux/files/home/MiuiserPeruser/Database/april.bin"
#define APRIL_SIZE 4096

#define APRIL_ROUTE_MODE    0
#define APRIL_KRANG_MODE    4
#define APRIL_LOG_LEVEL     8
#define APRIL_LOAD_MONITOR  12
#define APRIL_TCP_FALLBACK  16
#define APRIL_SYSTEM_LOCK   20
#define APRIL_POLL_THROTTLE 24

#define ROUTE_AUTO      0
#define ROUTE_UNIX_ONLY 1
#define ROUTE_TCP_ONLY  2

#define KRANG_ACTIVE       0
#define KRANG_PASS_THROUGH 1
#define KRANG_OFFLINE      2

#define LOG_MINIMAL 0
#define LOG_NORMAL  1
#define LOG_VERBOSE 2

#define SYSLOCK_NORMAL 0
#define SYSLOCK_LOCKED 1

#define POLL_NORMAL    0
#define POLL_THROTTLED 1
#define POLL_MINIMAL   2

uint32_t april_read(int offset, uint32_t default_val);
unsigned int april_poll_sleep(unsigned int base_seconds);

#endif
