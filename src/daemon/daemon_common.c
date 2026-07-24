#include "../core/log_safe.h"
#include "daemon_common.h"
/* put shared helpers here if needed; keep minimal for now */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

/* Only one weak attribute is needed */
__attribute__((weak))
const char *device_get_property(const char *key) {
    log_event(LOG_LEVEL_DEBUG, "COMMON", "device_get_property key=%s", key ? key : "(null)");
    return "";
}

FILE *results_open(const char *daemon, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[%s] ERROR: cannot write %s: %s\n",
                daemon, path, strerror(errno));
        log_event(LOG_LEVEL_ERROR, "RESULTS",
                  "%s: open failed: %s: %s", daemon, path, strerror(errno));
    }
    return f;
}

int results_close(const char *daemon, const char *path, FILE *f) {
    if (!f) return -1;

    /* fclose() alone hides short writes: the failure surfaces on the
     * buffered fprintf() calls, and only ferror()/fclose() report it. */
    int failed = ferror(f);
    int saved_errno = errno;

    if (fclose(f) != 0) {
        failed = 1;
        saved_errno = errno;
    }

    if (failed) {
        fprintf(stderr, "[%s] ERROR: truncated write to %s: %s\n",
                daemon, path, strerror(saved_errno));
        log_event(LOG_LEVEL_ERROR, "RESULTS",
                  "%s: write failed: %s: %s", daemon, path,
                  strerror(saved_errno));
        return -1;
    }
    return 0;
}

int splinter_emit(const char *daemon, const char *socket_path,
                  const char *type, const char *payload) {
    static int warned = 0;
    const char *stage = NULL;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        stage = "socket";
    } else {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            stage = "connect";
        } else {
            char buf[512];
            int n = snprintf(buf, sizeof(buf), "APRIL|%s|%s|%s\n",
                             daemon, type, payload ? payload : "");
            if (n <= 0 || n >= (int)sizeof(buf)) {
                errno = EMSGSIZE;
                stage = "format";
            } else if (write(fd, buf, (size_t)n) != n) {
                stage = "write";
            }
        }
        close(fd);
    }

    if (!stage) {
        warned = 0;
        return 0;
    }

    if (!warned) {
        warned = 1;
        fprintf(stderr, "[%s] event dropped (%s: %s): %s\n",
                daemon, stage, strerror(errno), type);
        log_event(LOG_LEVEL_WARN, "SPLINTER",
                  "%s: event dropped at %s: %s: %s",
                  daemon, stage, strerror(errno), type);
    }
    return -1;
}
