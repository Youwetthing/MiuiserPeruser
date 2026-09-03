#include "krangd_net.h"

#include <stdio.h>
#include <string.h>

static int expect(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

int main(void)
{
    /*
     * /proc/net/tcp6 uses little-endian 32-bit words. The first fixture
     * therefore encodes ::ffff:127.0.0.1 as ...FFFF00000100007F and
     * 8.8.8.8 as ...FFFF000008080808.
     */
    static const char fixture[] =
        "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when "
        "retrnsmt   uid  timeout inode\n"
        "   0: 0000000000000000FFFF00000100007F:C001 "
        "0000000000000000FFFF000008080808:01BB 01 "
        "00000000:00000000 00:00000000 00000000  10001        0 "
        "123456 1 0000000000000000 100 0 0 10 0\n"
        "   1: 0000000000000000FFFF00000100007F:C002 "
        "0000000000000000FFFF000001023214:1466 01 "
        "00000000:00000000 00:00000000 00000000  10212        0 "
        "123457 1 0000000000000000 100 0 0 10 0\n";
    krang_connection_t connections[2];
    size_t count = 0;
    parse_status_t status = krang_parse_tcp6(
        fixture, connections, 2, &count);

    printf("INPUT: tcp6 fixture with IPv4-mapped 8.8.8.8:443 and "
           "UID 10212 remote 20.50.2.1:5222\n");
    printf("OUTPUT: status=%s records=%zu\n",
           status == PARSE_FOUND ? "PARSE_FOUND" :
           status == PARSE_NOT_FOUND ? "PARSE_NOT_FOUND" : "PARSE_ERROR",
           count);
    for (size_t i = 0; i < count; i++) {
        printf("OUTPUT[%zu]: local=%s:%u remote=%s:%u state=%02X uid=%d "
               "inode=%llu anomaly=%s\n",
               i, connections[i].local_address, connections[i].local_port,
               connections[i].remote_address, connections[i].remote_port,
               connections[i].state, connections[i].uid, connections[i].inode,
               krang_is_nonstandard_app_connection(&connections[i]) ?
                   "candidate" : "no");
    }

    if (expect(status == PARSE_FOUND && count == 2,
               "fixture should produce two records") != 0)
        return 1;
    if (expect(strcmp(connections[0].remote_address, "8.8.8.8") == 0 &&
               connections[0].remote_port == 443,
               "first IPv4-mapped address must decode to 8.8.8.8:443") != 0)
        return 1;
    if (expect(connections[1].uid == 10212 &&
               strcmp(connections[1].remote_address, "20.50.2.1") == 0 &&
               connections[1].remote_port == 5222 &&
               krang_is_nonstandard_app_connection(&connections[1]),
               "UID 10212 on remote port 5222 must be flagged") != 0)
        return 1;

    printf("PASS: mapped-address decoding and xmsf-style anomaly candidate\n");
    return 0;
}