#ifndef KRANGD_NET_H
#define KRANGD_NET_H

#include <netinet/in.h>
#include <stddef.h>
#include "sensei_types.h"

/*
 * The parser status convention is deliberately tri-state:
 *   PARSE_FOUND     a record was parsed
 *   PARSE_NOT_FOUND the input was empty or contained only a header
 *   PARSE_ERROR     the input was malformed or an argument was invalid
 */
typedef enum {
    PARSE_FOUND     = SENSEI_STATUS_OK,
    PARSE_ERROR     = SENSEI_STATUS_ERROR,
    PARSE_NOT_FOUND = SENSEI_STATUS_NOT_FOUND
} parse_status_t;

typedef struct {
    char                 local_address[INET6_ADDRSTRLEN];
    unsigned int         local_port;
    char                 remote_address[INET6_ADDRSTRLEN];
    unsigned int         remote_port;
    unsigned int         state;
    int                  uid;
    unsigned long long   inode;
} krang_connection_t;

parse_status_t krang_parse_tcp6_line(const char *line,
                                     krang_connection_t *out);

parse_status_t krang_parse_tcp6(const char *text,
                                krang_connection_t *out,
                                size_t capacity,
                                size_t *parsed_count);

/*
 * The first version of krangd is intentionally conservative: app UIDs
 * using a remote port outside the ordinary web/DNS set are candidates for
 * reporting. This is a candidate signal, not a claim of maliciousness.
 */
int krang_is_nonstandard_app_connection(const krang_connection_t *connection);

#endif