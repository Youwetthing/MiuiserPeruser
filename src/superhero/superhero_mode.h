#ifndef SUPERHERO_MODE_H
#define SUPERHERO_MODE_H

#include <stdint.h>
#include "../core/include/sensei_types.h"

SENSEI_STATUS superhero_run_once(void);
SENSEI_STATUS superhero_run_loop(uint32_t interval_seconds);
SENSEI_STATUS superhero_run_n_times(uint32_t count, uint32_t interval_seconds);

#endif
