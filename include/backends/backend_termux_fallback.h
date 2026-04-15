#ifndef BACKEND_TERMUX_FALLBACK_H
#define BACKEND_TERMUX_FALLBACK_H
#include "backend_common.h"
extern const backend_info_t backend_termux_fallback_info;
int backend_termux_fallback_init(void);
void backend_termux_fallback_shutdown(void);
#endif
