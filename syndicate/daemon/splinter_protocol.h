#pragma once

/*
 * splinter_protocol.h — daemon-facing Splinter client API
 *
 * Include this to emit events to splinterd and probe the port bridge.
 * Wire format: APRIL|source|type|payload\n  (newline-delimited, no length prefix)
 *
 * This wraps splinter_emit.h using SPLINTER_SOCKET from ipc_globals
 * rather than a hardcoded path.
 */

#include "ipc_globals.h"

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SPLINTER_EMIT_BUF 2048

/*
 * splinter_emit — fire-and-forget event to splinterd
 *
 *   source:       identifying name of the sending daemon ("leatherheadd", etc.)
 *   type:         event type ("sysstate", "anomaly", "connectivity_anomaly", ...)
 *   payload:      freeform string (JSON, key=value, plain text — no newlines)
 */
static inline void splinter_emit(const char *source,
                                  const char *type,
                                  const char *payload)
{
    if (!source || !type || !payload) return;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return;
    }

    /* Sanitise payload — no newlines allowed in the wire format */
    char safe[SPLINTER_EMIT_BUF];
    strncpy(safe, payload, sizeof(safe) - 1);
    safe[sizeof(safe) - 1] = '\0';
    for (char *p = safe; *p; p++)
        if (*p == '\n' || *p == '\r') *p = ' ';

    char buf[SPLINTER_EMIT_BUF];
    int len = snprintf(buf, sizeof(buf), "APRIL|%s|%s|%s\n", source, type, safe);
    if (len > 0 && len < (int)sizeof(buf))
        write(fd, buf, (size_t)len);

    close(fd);
}

/*
 * splinter_protocol_probe — check if the port bridge is reachable
 * Probes splinterd's socket; returns 1 if connectable, 0 otherwise.
 */
static inline int splinter_protocol_probe(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return 0;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);

    int ok = (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) ? 1 : 0;
    close(fd);
    return ok;
}

/*
 * splinter_protocol_basic_info — placeholder until INFO exchange is defined
 * Returns 0 until the INFO command round-trip is implemented in splinterd.
 */
static inline int splinter_protocol_basic_info(void)
{
    return 0; /* TODO: send INFO event, read back confirmation */
}
