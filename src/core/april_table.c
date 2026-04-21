#include <fcntl.h>
#include <unistd.h>
#include "april_runtime.h"
#include "april_constants.h"

uint32_t april_read(APRIL_KEY key, APRIL_POLL_MODE mode);

/* safe wrapper if you still want file backing later */
uint32_t april_read(APRIL_KEY key, APRIL_POLL_MODE mode)
{
    return april_read(key, mode);
}
