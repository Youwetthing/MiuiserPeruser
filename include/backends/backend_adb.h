#ifndef BACKEND_ADB_H
#define BACKEND_ADB_H
#include "backend_common.h"
extern const backend_info_t backend_adb_info;
int backend_adb_init(void);
void backend_adb_shutdown(void);
#endif
