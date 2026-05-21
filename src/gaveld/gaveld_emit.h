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

#include <fcntl.h>
#include <stdio.h>
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
    /* Non-blocking open — returns ENXIO immediately if no reader */
    int fd = open(GAVELD_INGEST_PIPE, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return -1;

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
    if (n > 0 && n < (int)sizeof(buf))
        rc = (write(fd, buf, (size_t)n) == n) ? 0 : -1;

    close(fd);
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
