#ifndef GAVELD_EMIT_H
#define GAVELD_EMIT_H

/*
 * gaveld_emit.h — header-only helper for daemon → gaveld signal emission
 *
 * Usage:
 *   #include "gaveld_emit.h"
 *   gaveld_emit("rocksteadyd", "MIUI_OPTIMIZATION_OFF", 0.0, "pkg=com.example");
 *
 * Wire format:  SOURCE|SIGNAL|WEIGHT|CONTEXT\n
 * Transport:    FIFO at GAVELD_INGEST_PIPE (write-only, non-blocking)
 *
 * Non-blocking by design — if gaveld is not running, emit returns -1
 * immediately rather than hanging the calling daemon.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Override at compile time with -DGAVELD_INGEST_PIPE='"your/path"'
 * Must match INGEST_PIPE in src/gaveld/config.h.
 */
#ifndef GAVELD_INGEST_PIPE
#define GAVELD_INGEST_PIPE "pipes/ingest.pipe"
#endif

/*
 * gaveld_emit — send one signal record to gaveld.
 *
 *   source   daemon name,   e.g. "rocksteadyd"
 *   signal   signal name,   e.g. "MIUI_OPTIMIZATION_OFF"
 *   weight   0.0 = defer to weights table; positive = override
 *   ctx      optional context string; NULL or "" for none
 *
 * Returns 0 on success, -1 on error (gaveld not running, pipe missing, etc.)
 */
static inline int gaveld_emit(const char *source, const char *signal,
                               double weight, const char *ctx) {
    /*
     * Every call site ignores the return value, so a dropped signal used
     * to be completely invisible: the daemon logs the finding, gaveld
     * never scores it, and nothing says why. Report the first drop per
     * process (and the first after a recovery) instead of once per poll,
     * which would flood the log while gaveld is down.
     */
    static int warned = 0;

    /* Non-blocking open — returns ENXIO immediately if no reader */
    int fd = open(GAVELD_INGEST_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (!warned) {
            warned = 1;
            fprintf(stderr, "[GAVELD] signal dropped (%s: %s): %s/%s\n",
                    GAVELD_INGEST_PIPE, strerror(errno),
                    source ? source : "unknown", signal ? signal : "");
        }
        return -1;
    }

    /*
     * Buffer sized for worst-case:
     *   INGEST_SOURCE_MAX (256) + INGEST_SIGNAL_MAX (64) +
     *   INGEST_CTX_MAX (512) + separators + weight + newline
     */
    char buf[900];
    int n = snprintf(buf, sizeof(buf), "%s|%s|%.6g|%s\n",
                     source ? source : "unknown",
                     signal ? signal : "",
                     weight,
                     ctx    ? ctx    : "");

    int rc = -1;
    int write_errno = 0;
    if (n <= 0 || n >= (int)sizeof(buf)) {
        write_errno = EMSGSIZE;   /* record truncated before it was sent */
    } else if (write(fd, buf, (size_t)n) == n) {
        rc = 0;
        warned = 0;               /* re-arm: report the next drop too */
    } else {
        write_errno = errno;      /* short write or EAGAIN on a full pipe */
    }

    close(fd);

    if (rc != 0 && !warned) {
        warned = 1;
        fprintf(stderr, "[GAVELD] signal dropped (write: %s): %s/%s\n",
                strerror(write_errno),
                source ? source : "unknown", signal ? signal : "");
    }
    return rc;
}

/*
 * gaveld_emit_signal — send a signal using the weights table (weight = 0).
 * Convenience wrapper for the common case.
 *
 *   gaveld_emit_signal("bebopd", "BATTERY_SAVER_ACTIVE");
 */
static inline int gaveld_emit_signal(const char *source, const char *signal) {
    return gaveld_emit(source, signal, 0.0, "");
}

/*
 * gaveld_emit_ctx — send a signal with context, weight from table.
 *
 *   gaveld_emit_ctx("rocksteadyd", "APP_FROZEN", "pkg=com.miui.home");
 */
static inline int gaveld_emit_ctx(const char *source, const char *signal,
                                   const char *ctx) {
    return gaveld_emit(source, signal, 0.0, ctx);
}

/*
 * gaveld_emit_weighted — send a signal with an explicit weight override.
 *
 *   gaveld_emit_weighted("rahzerd", "RECIDIVISM_SPIKE", 0.9);
 */
static inline int gaveld_emit_weighted(const char *source, const char *signal,
                                        double weight) {
    return gaveld_emit(source, signal, weight, "");
}

#endif /* GAVELD_EMIT_H */
