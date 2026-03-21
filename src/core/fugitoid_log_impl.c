#include "fugitoid_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

static void iso_ts(char *buf, size_t n) {
  time_t t = time(NULL);
  struct tm tm;
  gmtime_r(&t, &tm);
  strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

static void json_escape(const char *in, char *out, size_t n) {
  size_t j = 0;
  for (size_t i = 0; in[i] && j + 6 < n; ++i) {
    unsigned char c = (unsigned char)in[i];
    if (c == '\"' || c == '\\') { out[j++] = '\\'; out[j++] = c; }
    else if (c >= 0x20) out[j++] = c;
    else { j += snprintf(out + j, n - j, "\\u%04x", c); }
  }
  out[j] = '\0';
}

void fugitoid_log_json(const char *level, const char *domain, const char *component,
                       const char *event, const char *correlation_id, const char *msg, const char *meta_json) {
  char ts[32]; iso_ts(ts, sizeof(ts));
  char esc_msg[1024]; json_escape(msg ? msg : "", esc_msg, sizeof(esc_msg));
  char esc_level[64]; json_escape(level ? level : "", esc_level, sizeof(esc_level));
  char esc_domain[128]; json_escape(domain ? domain : "", esc_domain, sizeof(esc_domain));
  char esc_component[128]; json_escape(component ? component : "", esc_component, sizeof(esc_component));
  char esc_event[128]; json_escape(event ? event : "", esc_event, sizeof(esc_event));
  char esc_cid[128]; json_escape(correlation_id ? correlation_id : "", esc_cid, sizeof(esc_cid));
  const char *meta = (meta_json && meta_json[0]) ? meta_json : "{}";
  fprintf(stderr, "{\"ts\":\"%s\",\"level\":\"%s\",\"domain\":\"%s\",\"component\":\"%s\",\"event\":\"%s\",\"correlation_id\":\"%s\",\"msg\":\"%s\",\"meta\":%s}\n",
          ts, esc_level, esc_domain, esc_component, esc_event, esc_cid, esc_msg, meta);
  fflush(stderr);
}
/* Compatibility wrapper for legacy fugitoid_log calls.
   Forwards formatted text into the structured fugitoid_log_json API. */
#include <stdarg.h>
#include <stdio.h>

void fugitoid_log(const char *level, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    /* Use UNKNOWN domain/component/event for legacy calls; meta empty. */
    fugitoid_log_json(level, "unknown", "legacy", "log", "", buf, "{}");
}
