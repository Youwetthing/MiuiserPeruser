#ifndef KRANGD_UID_H
#define KRANGD_UID_H

#include "krangd_net.h"

#include <stddef.h>

parse_status_t krang_parse_package_output(const char *output,
                                          char *package,
                                          size_t package_size);

parse_status_t krang_resolve_uid_package(int uid,
                                         char *package,
                                         size_t package_size);

#endif