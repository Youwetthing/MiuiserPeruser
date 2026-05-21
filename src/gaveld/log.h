#ifndef GAVELD_LOG_H
#define GAVELD_LOG_H

/*
 * log.h — thread-safe logging with rotation
 * All gaveld threads call glog() — serialised via mutex.
 * Rotates at LOG_MAX_BYTES (defined in config.h).
 */

int  log_open(void);
void log_close(void);

/* Primary log call — all threads use this */
void glog(const char *level, const char *fmt, ...);

/* Flush without closing — called by audit thread */
void log_flush(void);

#endif /* GAVELD_LOG_H */
