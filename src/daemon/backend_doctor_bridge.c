#include <string.h>
#include "backend_doctor.h"   // your existing doctor API

int doctor_get_status(char *buf, size_t buflen)
{
    if (!buf || buflen == 0)
        return -1;

    // Call your existing doctor function
    int rc = backend_doctor_status(buf, buflen);

    // Ensure NUL termination
    buf[buflen - 1] = '\0';

    return rc;
}
