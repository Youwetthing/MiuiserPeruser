#include "krangd_net.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define KRANG_TCP6_LINE_MAX 512

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * Linux prints each 32-bit word of an IPv6 address in host byte order in
 * /proc/net/tcp6. Reverse the bytes inside each eight-character word to
 * recover the network-order byte sequence expected by inet_ntop().
 */
static int decode_proc_address(const char *encoded,
                               char *decoded,
                               size_t decoded_size)
{
    struct in6_addr address;

    if (!encoded || !decoded || decoded_size == 0 ||
        strlen(encoded) != 32)
        return -1;

    for (size_t word = 0; word < 4; word++) {
        for (size_t byte = 0; byte < 4; byte++) {
            size_t source = word * 8 + (3 - byte) * 2;
            int high = hex_value(encoded[source]);
            int low  = hex_value(encoded[source + 1]);
            if (high < 0 || low < 0) return -1;
            address.s6_addr[word * 4 + byte] =
                (unsigned char)((high << 4) | low);
        }
    }

    if (IN6_IS_ADDR_V4MAPPED(&address)) {
        return inet_ntop(AF_INET, address.s6_addr + 12,
                         decoded, decoded_size) ? 0 : -1;
    }
    return inet_ntop(AF_INET6, &address, decoded, decoded_size) ? 0 : -1;
}

static int decode_endpoint(const char *encoded,
                           char *address,
                           size_t address_size,
                           unsigned int *port)
{
    char address_hex[33];
    char port_hex[5];
    unsigned int parsed_port = 0;

    if (!encoded || !address || !port) return -1;
    if (sscanf(encoded, "%32[0-9A-Fa-f]:%4[0-9A-Fa-f]",
               address_hex, port_hex) != 2 ||
        strlen(address_hex) != 32 || strlen(port_hex) != 4)
        return -1;

    for (size_t i = 0; i < 4; i++) {
        int digit = hex_value(port_hex[i]);
        if (digit < 0) return -1;
        parsed_port = (parsed_port << 4) | (unsigned int)digit;
    }

    if (decode_proc_address(address_hex, address, address_size) != 0)
        return -1;
    *port = parsed_port;
    return 0;
}

parse_status_t krang_parse_tcp6_line(const char *line,
                                     krang_connection_t *out)
{
    unsigned int slot = 0;
    unsigned int state = 0;
    unsigned int retransmit = 0;
    int uid = 0;
    int timeout = 0;
    unsigned long long inode = 0;
    char local[64], remote[64], tx_queue[64], timer[64];
    int fields;

    if (!line || !out) return PARSE_ERROR;

    while (isspace((unsigned char)*line)) line++;
    if (*line == '\0' || strncmp(line, "sl ", 3) == 0)
        return PARSE_NOT_FOUND;

    fields = sscanf(line,
                    "%u: %63s %63s %x %63s %63s %x %d %d %llu",
                    &slot, local, remote, &state, tx_queue, timer,
                    &retransmit, &uid, &timeout, &inode);
    if (fields != 10) return PARSE_ERROR;

    if (decode_endpoint(local, out->local_address,
                        sizeof(out->local_address), &out->local_port) != 0 ||
        decode_endpoint(remote, out->remote_address,
                        sizeof(out->remote_address), &out->remote_port) != 0)
        return PARSE_ERROR;

    out->state = state;
    out->uid = uid;
    out->inode = inode;
    return PARSE_FOUND;
}

parse_status_t krang_parse_tcp6(const char *text,
                                krang_connection_t *out,
                                size_t capacity,
                                size_t *parsed_count)
{
    const char *cursor;
    size_t count = 0;
    int saw_error = 0;

    if (!text || (!out && capacity != 0) || !parsed_count)
        return PARSE_ERROR;

    *parsed_count = 0;
    cursor = text;
    while (*cursor) {
        const char *end = strchr(cursor, '\n');
        size_t length = end ? (size_t)(end - cursor) : strlen(cursor);
        char line[KRANG_TCP6_LINE_MAX];
        krang_connection_t parsed;

        if (length >= sizeof(line)) return PARSE_ERROR;
        memcpy(line, cursor, length);
        line[length] = '\0';

        parse_status_t status = krang_parse_tcp6_line(line, &parsed);
        if (status == PARSE_ERROR) {
            saw_error = 1;
        } else if (status == PARSE_FOUND) {
            if (count < capacity) out[count] = parsed;
            count++;
        }

        if (!end) break;
        cursor = end + 1;
    }

    if (count > capacity) count = capacity;
    *parsed_count = count;
    if (saw_error) return PARSE_ERROR;
    return count > 0 ? PARSE_FOUND : PARSE_NOT_FOUND;
}

int krang_is_nonstandard_app_connection(const krang_connection_t *connection)
{
    if (!connection || connection->uid < 10000) return 0;
    return connection->remote_port != 53 &&
           connection->remote_port != 80 &&
           connection->remote_port != 443;
}