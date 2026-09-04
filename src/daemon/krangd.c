/*
 * krangd — IPv6 connection attribution sensor
 *
 * This daemon reads /proc/net/tcp6 through the fleet's existing bexec()
 * backend, attributes app-owned UIDs to package names, and reports
 * non-standard remote ports as candidates for further investigation.
 */

#include "daemon_core.h"
#include "backend_exec.h"
#include "ipc_globals.h"
#include "krangd_net.h"
#include "krangd_uid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define DAEMON_NAME "krangd"
#define DEFAULT_POLL_SEC 30
#define MAX_CONNECTIONS 256

static void splinterd_emit(const char *type, const char *payload)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, SPLINTER_SOCKET,
            sizeof(address.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
        char message[512];
        int length = snprintf(message, sizeof(message),
                               "APRIL|" DAEMON_NAME "|%s|%s\n",
                               type, payload);
        if (length > 0) write(fd, message, (size_t)length);
    }
    close(fd);
}

static int poll_seconds(void)
{
    const char *value = getenv("KRANGD_POLL_SEC");
    char *end = NULL;
    long parsed;

    if (!value || !*value) return DEFAULT_POLL_SEC;
    parsed = strtol(value, &end, 10);
    if (*end != '\0' || parsed < 1 || parsed > 3600)
        return DEFAULT_POLL_SEC;
    return (int)parsed;
}

static void poll_connections(void)
{
    char *raw = bexec("cat /proc/net/tcp6 2>/dev/null");
    krang_connection_t connections[MAX_CONNECTIONS];
    size_t count = 0;
    parse_status_t status;

    if (!raw) {
        daemon_log_warn("cannot read /proc/net/tcp6 through backend");
        return;
    }

    status = krang_parse_tcp6(raw, connections, MAX_CONNECTIONS, &count);
    free(raw);
    if (status == PARSE_ERROR) {
        daemon_log_warn("malformed /proc/net/tcp6 data; discarded poll");
        return;
    }

    for (size_t i = 0; i < count; i++) {
        char package[128];
        parse_status_t package_status;

        if (connections[i].state != 0x01 ||
            !krang_is_nonstandard_app_connection(&connections[i]))
            continue;

        package_status = krang_resolve_uid_package(
            connections[i].uid, package, sizeof(package));
        if (package_status != PARSE_FOUND)
            snprintf(package, sizeof(package), "uid-%d", connections[i].uid);

        char payload[384];
        snprintf(payload, sizeof(payload),
                 "package=%s uid=%d remote=%s:%u state=%02X",
                 package, connections[i].uid, connections[i].remote_address,
                 connections[i].remote_port, connections[i].state);
        splinterd_emit("TCP6_ANOMALY", payload);
        daemon_log_info(
            "candidate package=%s uid=%d remote=%s:%u state=%02X",
            package, connections[i].uid, connections[i].remote_address,
            connections[i].remote_port, connections[i].state);
    }
}

int main(void)
{
    int interval;

    if (!daemon_core_init(DAEMON_NAME)) return 1;
    interval = poll_seconds();
    bexec_init();
    daemon_log_info("krangd online; poll_sec=%d", interval);

    for (;;) {
        poll_connections();
        sleep(interval);
    }
    daemon_log_info("krangd shutdown");
    daemon_core_shutdown();
    return 0;
}