/*
 * rahzerd.c  v2  —  Full-Spectrum Connectivity Audit Daemon
 *
 * Env:
 *   RAHZERD_POLL_SEC    poll interval seconds  (default 15, min 5)
 *   RAHZERD_SPLINTER    splinterd socket path
 *   RAHZERD_DEBUG       1 = verbose stderr
 *   RAHZERD_LOG_PATH    raw event log file
 *   RAHZERD_DNS_HOST    DNS test hostname      (default dns.google)
 *   RAHZERD_NO_RISH     1 = skip rish
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netdb.h>
#include <dirent.h>

#include "rahzerd.h"
#include "gaveld_emit.h"
volatile bool g_rahzerd_running = true;
#include "ipc_globals.h"

#include <stdbool.h>


#define RESULTS_FILE "/data/data/com.termux/files/home/MiuiserPeruser/Registry/daemon_results/rahzerd.json"
#define BASELINE_FILE "/data/data/com.termux/files/home/MiuiserPeruser/data/rahzerd_baseline.json"
#define DEFAULT_POLL_SEC  15
#define DEFAULT_DNS_HOST  "dns.google"
#define MAX_CMD_OUT       8192
#define EMIT_BUF          4096

static int           g_debug    = 0;

static int           g_poll_sec = DEFAULT_POLL_SEC;
static const char   *g_splinter = SPLINTER_SOCKET;
static const char   *g_dns_host = DEFAULT_DNS_HOST;
static FILE         *g_log_fp   = NULL;
static rz_backend_t  g_backend  = RZ_BACKEND_NONE;

static void rzlog(const char *lvl, const char *fmt, ...) {
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    va_list ap; va_start(ap, fmt);
    if (g_debug || strcmp(lvl,"ERROR")==0 || strcmp(lvl,"WARN")==0) {
        fprintf(stderr, "[%s][RAHZERD/%s] ", ts, lvl);
        vfprintf(stderr, fmt, ap); fprintf(stderr,"\n"); fflush(stderr);
    }
    if (g_log_fp) {
        fprintf(g_log_fp, "[%s][RAHZERD/%s] ", ts, lvl);
        vfprintf(g_log_fp, fmt, ap); fprintf(g_log_fp,"\n"); fflush(g_log_fp);
    }
    va_end(ap);
}

static void handle_sig(int s) { (void)s; g_rahzerd_running = 0; }

char *rz_run(const char *cmd) {
    /* Wrap command with timeout to prevent hanging */
    char safe_cmd[2048];
    snprintf(safe_cmd, sizeof(safe_cmd), "timeout 3 %s", cmd);
    FILE *f = popen(safe_cmd, "r");
    if (!f) return NULL;
    char *buf = malloc(MAX_CMD_OUT);
    if (!buf) { pclose(f); return NULL; }
    size_t tot = 0, n;
    while ((n = fread(buf + tot, 1, MAX_CMD_OUT - tot - 1, f)) > 0) {
        tot += n;
        if (tot >= (size_t)(MAX_CMD_OUT - 1)) break;
    }
    pclose(f);
    buf[tot] = '\0';
    if (!tot) { free(buf); return NULL; }
    return buf;
}

char *rz_extract_field(const char *hay, const char *key,
                        char *out, size_t outlen) {
    if (!hay || !key || !out || outlen < 2) return NULL;
    size_t klen = strlen(key);
    const char *p = hay;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncasecmp(p, key, klen) == 0) {
            const char *a = p + klen;
            while (*a == '=' || *a == ':' || *a == ' ' || *a == '\t') a++;
            size_t i = 0;
            while (*a && *a != '\n' && *a != '\r' && i < outlen - 1)
                out[i++] = *a++;
            while (i > 0 && (out[i-1] == ' ' || out[i-1] == '\t')) i--;
            out[i] = '\0';
            if (i > 0) return out;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return NULL;
}

static int rz_has(const char *h, const char *n) {
    return (h && n && strstr(h, n) != NULL);
}

static void rz_trim_nl(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    while (l > 0 && (s[l-1]=='\n'||s[l-1]=='\r'||s[l-1]==' ')) s[--l]='\0';
}

/* Pre-baked probe data — loaded once per poll */
static char g_probe_data[131072] = {0};
static int  g_probe_loaded = 0;

static void rz_load_probe_data(void) {
    FILE *f = fopen("/data/data/com.termux/files/home/MiuiserPeruser/pipes/state/rahzerd_all", "r");
    if (!f) { g_probe_loaded = 0; return; }
    memset(g_probe_data, 0, sizeof(g_probe_data));
    fread(g_probe_data, 1, sizeof(g_probe_data)-1, f);
    fclose(f);
    g_probe_loaded = 1;
}

static char *ds(const char *svc) {
    /* Return section from pre-baked data matching the service */
    if (!g_probe_loaded) return NULL;
    char section[32];
    snprintf(section, sizeof(section), "===%s===", svc);
    /* Uppercase the section name to match */
    for (int i = 0; section[i]; i++)
        if (section[i] >= 'a' && section[i] <= 'z') section[i] -= 32;
    char *start = strstr(g_probe_data, section);
    if (!start) return NULL;
    start = strchr(start, '\n');
    if (!start) return NULL;
    start++;
    /* Find next section */
    char *end = strstr(start, "\n===");
    size_t len = end ? (size_t)(end - start) : strlen(start);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = 0;
    return out;
}


static char *rz_prebaked_get(const char *key) {
    if (!g_probe_loaded || !key) return NULL;
    size_t klen = strlen(key);
    char *p = g_probe_data;
    while (*p) {
        while (*p == '\n' || *p == '\r') p++;
        if (!*p) break;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *val = p + klen + 1;
            size_t len = 0;
            while (val[len] && val[len] != '\n' && val[len] != '\r') len++;
            if (!len) { p += klen + 1; continue; }
            char *out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, val, len);
            out[len] = 0;
            return out;
        }
        while (*p && *p != '\n' && *p != '\r') p++;
    }
    return NULL;
}

static char *rz_getprop(const char *prop) {
    /* Check pre-baked data first */
    char *v = rz_prebaked_get(prop);
    if (v) { rz_trim_nl(v); return v; }
    /* Fallback: live via adb */
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "/data/data/com.termux/files/home/.cargo/bin/adb_cli "
        "tcp 127.0.0.1:5555 shell getprop %s 2>/dev/null", prop);
    char *r = rz_run(cmd);
    if (r) rz_trim_nl(r);
    return r;
}

static char *rz_sysfs(const char *path) {
    /* Check pre-baked data first */
    char *v = rz_prebaked_get(path);
    if (v) { rz_trim_nl(v); return v; }
    /* Fallback: read via adb */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "/data/data/com.termux/files/home/.cargo/bin/adb_cli "
        "tcp 127.0.0.1:5555 shell cat '%s' 2>/dev/null", path);
    char *r = rz_run(cmd);
    if (r) rz_trim_nl(r);
    return r;
}



static int sysfs_exists(const char *path) {
    return access(path, F_OK) == 0;
}

rz_backend_t rz_detect_backend(void) {
    /* Force rish backend — always available via Shizuku */
    rzlog("INFO", "backend=rish (forced)");
    return RZ_BACKEND_RISH;
    /* DEAD CODE BELOW — kept for reference */

    const char *no_rish = getenv("RAHZERD_NO_RISH");
    if (!no_rish || strcmp(no_rish, "1") != 0) {
        char *t = rz_run("RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c 'echo rish_ok' 2>/dev/null");
        if (t && strstr(t, "rish_ok")) { free(t);
            rzlog("INFO", "backend=rish"); return RZ_BACKEND_RISH; }
        free(t);
    }
    char *d = rz_run("RISH_APPLICATION_ID=com.termux /data/data/com.termux/files/home/Rish/rish -c 'dumpsys wifi 2>/dev/null'");
    if (d && strlen(d) > 32) { free(d);
        rzlog("INFO", "backend=dumpsys"); return RZ_BACKEND_DUMPSYS; }
    free(d);
    char *s = rz_prebaked_get("/proc/net/wireless");
    if (s) { free(s);
        rzlog("INFO", "backend=sysfs"); return RZ_BACKEND_SYSFS; }
    rzlog("WARN", "backend=none");
    return RZ_BACKEND_NONE;
}

/* ── Layer 1: WiFi ─────────────────────────────────────────────── */

void rz_probe_wifi(rz_wifi_t *w) {
    memset(w, 0, sizeof(*w));
    w->connected = w->link_speed_mbps = w->frequency_mhz = -1;
    w->confidence = RZ_CONF_ABSENT;

    /* Read from pre-baked state file */
    FILE *f = fopen("/data/data/com.termux/files/home/MiuiserPeruser/pipes/state/rahzerd_wifi", "r");
    if (f) {
        char buf[8] = {0};
        fgets(buf, sizeof(buf), f);
        fclose(f);
        buf[strcspn(buf, "\n\r")] = 0;
        w->connected = (buf[0] == '1') ? 1 : 0;
        w->backend    = RZ_BACKEND_SYSFS;
        w->confidence = RZ_CONF_MEASURED;
    }

    /* TX/RX from sysfs via ip */
    char *v;
    if ((v = rz_prebaked_get("/sys/class/net/wlan0/statistics/tx_bytes"))) { w->tx_bytes = atol(v); free(v); }
    if ((v = rz_prebaked_get("/sys/class/net/wlan0/statistics/rx_bytes"))) { w->rx_bytes = atol(v); free(v); }
}

void rz_probe_mobile(rz_mobile_t *m) {
    memset(m, 0, sizeof(*m));
    m->sim_present = m->data_active = m->roaming = -1;
    m->signal_strength = m->cell_id = m->dual_sim = -1;
    m->confidence = RZ_CONF_ABSENT;
    strncpy(m->rat_type,      "UNKNOWN", sizeof(m->rat_type)-1);
    strncpy(m->data_activity, "UNKNOWN", sizeof(m->data_activity)-1);

    FILE *f = fopen("/data/data/com.termux/files/home/MiuiserPeruser/pipes/state/rahzerd_mobile", "r");
    if (!f) return;
    char raw[8192] = {0};
    fread(raw, 1, sizeof(raw)-1, f);
    fclose(f);

    /* SIM state from last line */
    m->sim_present = rz_has(raw, "LOADED") ? 1 : 0;
    m->dual_sim    = rz_has(raw, "ABSENT,LOADED") || rz_has(raw, "LOADED,LOADED") ? 1 : 0;

    /* Data active — look for IN_SERVICE on WWAN PS domain */
    m->data_active = rz_has(raw, "domain=PS transportType=WWAN registrationState=HOME") ? 1 : 0;

    /* RAT type */
    if      (rz_has(raw, "getRilDataRadioTechnology=14")) strncpy(m->rat_type, "LTE",  sizeof(m->rat_type)-1);
    else if (rz_has(raw, "getRilDataRadioTechnology=20")) strncpy(m->rat_type, "NR",   sizeof(m->rat_type)-1);
    else if (rz_has(raw, "getRilDataRadioTechnology=11")) strncpy(m->rat_type, "HSPA+",sizeof(m->rat_type)-1);

    /* Operator */
    const char *op = strstr(raw, "mOperatorAlphaLongRaw=");
    if (op) {
        op += 22;
        if (strncmp(op, "null", 4) != 0) {
            int i = 0;
            while (op[i] && op[i] != ',' && op[i] != '}' && op[i] != '\n' &&
                   i < (int)sizeof(m->operator_name)-1)
                m->operator_name[i] = op[i++];
        }
    }

    /* Roaming */
    m->roaming = rz_has(raw, "roamingType=ROAMING") ? 1 : 0;

    m->backend    = RZ_BACKEND_RISH;
    m->confidence = RZ_CONF_INFERRED;
}

/* ── Layer 3: DNS ──────────────────────────────────────────────── */

#define DNS_TIMEOUT_SEC 5
static volatile sig_atomic_t g_dns_timed_out = 0;
static void dns_alarm_handler(int s) { (void)s; g_dns_timed_out = 1; }

void rz_probe_dns(rz_dns_t *d) {
    memset(d, 0, sizeof(*d));
    d->resolves = d->latency_ms = d->private_dns_active = -1;
    d->confidence = RZ_CONF_ABSENT;
    strncpy(d->test_host, g_dns_host, sizeof(d->test_host)-1);

    char *v;
    if ((v = rz_prebaked_get("net.dns1"))) {
        if (strlen(v)) strncpy(d->nameserver_primary,   v, sizeof(d->nameserver_primary)-1);
        free(v);
    }
    if ((v = rz_prebaked_get("net.dns2"))) {
        if (strlen(v)) strncpy(d->nameserver_secondary, v, sizeof(d->nameserver_secondary)-1);
        free(v);
    }
    if ((v = rz_prebaked_get("persist.private_dns_mode"))) {
        d->private_dns_active = (strcmp(v,"opportunistic")==0||strcmp(v,"hostname")==0) ? 1 : 0;
        free(v);
    }
    if ((v = rz_prebaked_get("persist.dns.mode.hostname"))) {
        if (strlen(v)) strncpy(d->private_dns_host, v, sizeof(d->private_dns_host)-1);
        free(v);
    }

    g_dns_timed_out = 0;
    struct sigaction sa_new, sa_old;
    memset(&sa_new, 0, sizeof(sa_new));
    sa_new.sa_handler = dns_alarm_handler;
    sigemptyset(&sa_new.sa_mask);
    sigaction(SIGALRM, &sa_new, &sa_old);
    alarm(DNS_TIMEOUT_SEC);

    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(g_dns_host, NULL, &hints, &res);
    gettimeofday(&t1, NULL);

    alarm(0);
    sigaction(SIGALRM, &sa_old, NULL);

    long ms = (t1.tv_sec - t0.tv_sec)*1000L + (t1.tv_usec - t0.tv_usec)/1000L;
    d->latency_ms  = (int)ms;
    d->backend     = RZ_BACKEND_SYSFS;
    d->confidence  = RZ_CONF_MEASURED;

    if (g_dns_timed_out) {
        d->resolves   = 0;
        d->latency_ms = DNS_TIMEOUT_SEC * 1000;
        rzlog("WARN", "DNS timeout (>%ds) for %s", DNS_TIMEOUT_SEC, g_dns_host);
    } else {
        d->resolves = (rc == 0 && res != NULL) ? 1 : 0;
        if (res) freeaddrinfo(res);
        if (!d->resolves)
            rzlog("WARN", "DNS fail: %s (%s)", g_dns_host, gai_strerror(rc));
    }
}

/* ── Layer 4: Telephone ────────────────────────────────────────── */

void rz_probe_telephone(rz_telephone_t *t) {
    memset(t, 0, sizeof(*t));
    t->call_active = t->call_ringing = t->call_holding = -1;
    t->confidence  = RZ_CONF_ABSENT;
    strncpy(t->call_state,     "UNKNOWN", sizeof(t->call_state)-1);
    strncpy(t->call_direction, "UNKNOWN", sizeof(t->call_direction)-1);
    strncpy(t->network_state,  "UNKNOWN", sizeof(t->network_state)-1);

    char *raw = ds("telecom");
    if (raw) {
        t->call_active  = rz_has(raw,"ACTIVE")  ? 1 : 0;
        t->call_ringing = rz_has(raw,"RINGING") ? 1 : 0;
        t->call_holding = rz_has(raw,"HOLDING") ? 1 : 0;
        if      (t->call_ringing) strncpy(t->call_state,"RINGING",sizeof(t->call_state)-1);
        else if (t->call_active)  strncpy(t->call_state,"OFFHOOK",sizeof(t->call_state)-1);
        else                      strncpy(t->call_state,"IDLE",   sizeof(t->call_state)-1);
        if      (rz_has(raw,"INCOMING")) strncpy(t->call_direction,"IN",  sizeof(t->call_direction)-1);
        else if (rz_has(raw,"OUTGOING")) strncpy(t->call_direction,"OUT", sizeof(t->call_direction)-1);
        else                             strncpy(t->call_direction,"NONE",sizeof(t->call_direction)-1);
        t->backend    = g_backend;
        t->confidence = RZ_CONF_INFERRED;
        free(raw);
    }
    char *tel = ds("telephony.registry");
    if (tel) {
        t->emergency_only = rz_has(tel,"EMERGENCY_ONLY") ? 1 : 0;
        if      (rz_has(tel,"IN_SERVICE"))     strncpy(t->network_state,"IN_SERVICE",sizeof(t->network_state)-1);
        else if (rz_has(tel,"EMERGENCY_ONLY")) strncpy(t->network_state,"EMERGENCY", sizeof(t->network_state)-1);
        else if (rz_has(tel,"OUT_OF_SERVICE")) strncpy(t->network_state,"OOS",       sizeof(t->network_state)-1);
        free(tel);
    }
}

/* ── Layer 5: SMS ──────────────────────────────────────────────── */

void rz_probe_sms(rz_sms_t *s) {
    memset(s, 0, sizeof(*s));
    s->service_available = s->sms_capable = s->sending_blocked = -1;
    s->confidence = RZ_CONF_ABSENT;
    strncpy(s->service_state, "UNKNOWN", sizeof(s->service_state)-1);

    char *sim = rz_prebaked_get("gsm.sim.state");
    if (sim) {
        s->service_available = rz_has(sim,"READY") ? 1 : 0;
        s->sms_capable       = s->service_available;
        if      (rz_has(sim,"READY"))  strncpy(s->service_state,"IN_SERVICE",sizeof(s->service_state)-1);
        else if (rz_has(sim,"ABSENT")) strncpy(s->service_state,"NO_SIM",    sizeof(s->service_state)-1);
        s->backend    = RZ_BACKEND_SYSFS;
        s->confidence = RZ_CONF_INFERRED;
        free(sim);
    }
    char *raw = ds("isms");
    if (raw) {
        s->sending_blocked = (rz_has(raw,"blocked")||rz_has(raw,"BLOCKED")) ? 1 : 0;
        free(raw);
    }
}

/* ── Layer 6: MMS + APN ────────────────────────────────────────── */

void rz_probe_mms(rz_mms_t *m) {
    memset(m, 0, sizeof(*m));
    m->mms_capable = m->apn_active = -1;
    m->confidence  = RZ_CONF_ABSENT;

    char *raw = ds("imms");
    if (!raw) raw = ds("telephony");
    if (raw) {
        char tmp[256];
        m->mms_capable = (rz_has(raw,"mms")||rz_has(raw,"MMS")) ? 1 : 0;
        m->apn_active  = (rz_has(raw,"APN_TYPE_MMS")||rz_has(raw,"mms connected")) ? 1 : 0;
        if (rz_extract_field(raw,"apn",     tmp,sizeof(tmp))) strncpy(m->apn_name,  tmp,sizeof(m->apn_name)-1);
        if (rz_extract_field(raw,"mmsc",    tmp,sizeof(tmp))) strncpy(m->mmsc_url,  tmp,sizeof(m->mmsc_url)-1);
        if (rz_extract_field(raw,"mmsProxy",tmp,sizeof(tmp))) strncpy(m->mms_proxy, tmp,sizeof(m->mms_proxy)-1);
        if (rz_extract_field(raw,"mmsPort", tmp,sizeof(tmp))) m->mms_port = atoi(tmp);
        m->backend    = g_backend;
        m->confidence = RZ_CONF_INFERRED;
        free(raw);
    }
}

/* ── Layer 7: Network Ports ────────────────────────────────────── */

static int port_from_hex(const char *hex) {
    unsigned int v = 0; sscanf(hex, "%X", &v); return (int)v;
}

static void probe_tcp_file(const char *path, rz_ports_t *p, int is_v6) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[512];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    while (fgets(line, sizeof(line), f)) {
        char local[72], rem[72], shex[4];
        if (sscanf(line, " %*d: %71s %71s %3s", local, rem, shex) != 3) continue;
        unsigned int state = 0; sscanf(shex, "%X", &state);
        char *cl = strrchr(local, ':'); if (!cl) continue;
        char *cr = strrchr(rem,   ':'); if (!cr) continue;
        int lport = port_from_hex(cl+1);
        int rport = port_from_hex(cr+1);
        (void)is_v6;
        if (state == 0x0A) {
            if (p->listen_count >= 0 && p->listen_count < RZ_MAX_OPEN_PORTS)
                p->listen_ports[p->listen_count++] = lport;
            else if (p->listen_count == RZ_MAX_OPEN_PORTS)
                p->listen_count = RZ_MAX_OVERFLOW_FLAG;
            if (lport > 1024 && lport != 8080 && lport != 8443)
                p->suspicious_listeners++;
        } else if (state == 0x01) {
            if (!is_v6) p->established_tcp4++; else p->established_tcp6++;
            switch (rport) {
                case 25:  p->smtp_25  = 1; break;
                case 465: p->smtp_465 = 1; break;
                case 587: p->smtp_587 = 1; break;
                case 143: p->imap_143 = 1; break;
                case 993: p->imap_993 = 1; break;
                case 110: p->pop3_110 = 1; break;
                case 995: p->pop3_995 = 1; break;
                default:  break;
            }
        }
    }
    fclose(f);
}

void rz_probe_ports(rz_ports_t *p) {
    memset(p, 0, sizeof(*p));
    p->confidence = RZ_CONF_ABSENT;

    /* Read from pre-baked PORTS section — two lines: tcp4_count tcp6_count */
    char *ports_raw = ds("PORTS");
    if (ports_raw) {
        int tcp4 = 0, tcp6 = 0;
        char *nl = strchr(ports_raw, '\n');
        tcp4 = atoi(ports_raw);
        if (nl) tcp6 = atoi(nl + 1);
        p->established_tcp4 = tcp4 > 1 ? tcp4 - 1 : 0;
        p->established_tcp6 = tcp6 > 1 ? tcp6 - 1 : 0;
        p->listen_count     = p->established_tcp4 + p->established_tcp6;
        p->confidence = RZ_CONF_MEASURED;
        free(ports_raw);
    }

    /* Also try direct read */
    probe_tcp_file("/proc/net/tcp",  p, 0);
    probe_tcp_file("/proc/net/tcp6", p, 1);
    p->backend = RZ_BACKEND_SYSFS;
}

/* ── Layer 8: Ethernet ─────────────────────────────────────────── */

void rz_probe_ethernet(rz_ethernet_t *e) {
    memset(e, 0, sizeof(*e));
    e->present = e->connected = e->speed_mbps = -1;
    e->confidence = RZ_CONF_ABSENT;
    if (!sysfs_exists("/sys/class/net/eth0")) { e->present = 0; return; }
    e->present = 1;
    char *op = rz_prebaked_get("/sys/class/net/eth0/operstate");
    if (op) { e->connected = rz_has(op,"up") ? 1 : 0; free(op); }
    char *v;
    if ((v = rz_prebaked_get("/sys/class/net/eth0/address")))             { strncpy(e->mac, v, sizeof(e->mac)-1); free(v); }
    if ((v = rz_prebaked_get("/sys/class/net/eth0/statistics/tx_bytes"))) { e->tx_bytes   = atol(v); free(v); }
    if ((v = rz_prebaked_get("/sys/class/net/eth0/statistics/rx_bytes"))) { e->rx_bytes   = atol(v); free(v); }
    if ((v = rz_prebaked_get("/sys/class/net/eth0/speed")))               { e->speed_mbps = atoi(v); free(v); }
    e->backend    = RZ_BACKEND_SYSFS;
    e->confidence = RZ_CONF_MEASURED;
}

/* ── Layer 9: UWB ──────────────────────────────────────────────── */

void rz_probe_uwb(rz_uwb_t *u) {
    memset(u, 0, sizeof(*u));
    u->supported = u->enabled = u->ranging_active = -1;
    u->confidence = RZ_CONF_ABSENT;
    strncpy(u->state, "UNKNOWN", sizeof(u->state)-1);
    char *raw = ds("uwb");
    if (raw) {
        u->supported      = 1;
        u->enabled        = rz_has(raw,"enabled=true")   ? 1 : 0;
        u->ranging_active = rz_has(raw,"RANGING_ACTIVE") ? 1 : 0;
        if (rz_has(raw,"IDLE"))   strncpy(u->state,"IDLE",  sizeof(u->state)-1);
        if (rz_has(raw,"ACTIVE")) strncpy(u->state,"ACTIVE",sizeof(u->state)-1);
        u->backend    = g_backend;
        u->confidence = RZ_CONF_INFERRED;
        free(raw);
    } else { u->supported = 0; }
}

/* ── Layer 10: FM Radio ────────────────────────────────────────── */

void rz_probe_fm(rz_fm_t *f) {
    memset(f, 0, sizeof(*f));
    f->supported = f->enabled = -1;
    f->confidence = RZ_CONF_ABSENT;
    strncpy(f->state, "UNKNOWN", sizeof(f->state)-1);
    char *v = rz_prebaked_get("ro.fm.type");
    if (v) { f->supported = (strlen(v)>0 && strcmp(v,"0")!=0) ? 1 : 0; free(v); }
    else     f->supported = 0;
    char *raw = ds("radio.fm");
    if (!raw) raw = ds("fm");
    if (raw) {
        f->enabled = rz_has(raw,"enabled=true") ? 1 : 0;
        f->stereo  = rz_has(raw,"stereo=true")  ? 1 : 0;
        char tmp[32];
        if (rz_extract_field(raw,"frequency",tmp,sizeof(tmp)))
            f->frequency_mhz = atof(tmp);
        f->backend    = g_backend;
        f->confidence = RZ_CONF_INFERRED;
        free(raw);
    }
}

/* ── Layer 11: USB ─────────────────────────────────────────────── */

void rz_probe_usb(rz_usb_t *u) {
    memset(u, 0, sizeof(*u));
    u->connected = u->adb_active = u->tethering_active = -1;
    u->confidence = RZ_CONF_ABSENT;
    strncpy(u->state, "UNKNOWN", sizeof(u->state)-1);
    strncpy(u->mode,  "UNKNOWN", sizeof(u->mode)-1);
    char *raw = ds("usb");
    if (raw) {
        u->connected        = rz_has(raw,"connected=true") ? 1 : 0;
        u->adb_active       = rz_has(raw,"adb")            ? 1 : 0;
        u->tethering_active = rz_has(raw,"tethering=true") ? 1 : 0;
        if      (rz_has(raw,"mtp"))      strncpy(u->mode,"MTP",     sizeof(u->mode)-1);
        else if (rz_has(raw,"ptp"))      strncpy(u->mode,"PTP",     sizeof(u->mode)-1);
        else if (rz_has(raw,"rndis"))    strncpy(u->mode,"RNDIS",   sizeof(u->mode)-1);
        else if (rz_has(raw,"charging")) strncpy(u->mode,"CHARGING",sizeof(u->mode)-1);
        u->backend    = g_backend;
        u->confidence = RZ_CONF_INFERRED;
        free(raw);
    }
    if (u->connected == -1) {
        char *st = rz_prebaked_get("/sys/class/android_usb/android0/state");
        if (st) {
            u->connected  = rz_has(st,"CONFIGURED") ? 1 : 0;
            u->backend    = RZ_BACKEND_SYSFS;
            u->confidence = RZ_CONF_FALLBACK;
            free(st);
        }
    }
}

/* ── Layer 12: Infrared ────────────────────────────────────────── */

void rz_probe_infrared(rz_infrared_t *ir) {
    memset(ir, 0, sizeof(*ir));
    ir->supported = ir->transmitting = -1;
    ir->confidence = RZ_CONF_ABSENT;
    const char *paths[] = {
        "/dev/lirc0", "/dev/ir_spi", "/dev/ttyS1",
        "/sys/class/misc/ir_hal0", NULL
    };
    ir->supported = 0;
    for (int i = 0; paths[i]; i++) {
        if (sysfs_exists(paths[i])) {
            ir->supported = 1;
            strncpy(ir->device_path, paths[i], sizeof(ir->device_path)-1);
            break;
        }
    }
    ir->transmitting = 0;
    ir->backend      = RZ_BACKEND_SYSFS;
    ir->confidence   = RZ_CONF_INFERRED;
}

/* ── Layer 13: Bluetooth ───────────────────────────────────────── */

void rz_probe_bluetooth(rz_bluetooth_t *b) {
    memset(b, 0, sizeof(*b));
    b->enabled = b->scanning = b->advertising = -1;
    b->paired_count = b->connected_count = -1;
    b->confidence = RZ_CONF_ABSENT;
    char *raw = ds("bluetooth_manager");
    if (!raw) raw = ds("bluetooth");
    if (raw) {
        b->enabled      = (rz_has(raw,"enabled=true")||rz_has(raw,"STATE_ON")) ? 1 : 0;
        b->scanning     = rz_has(raw,"isDiscovering=true") ? 1 : 0;
        b->advertising  = rz_has(raw,"isAdvertising=true") ? 1 : 0;
        b->le_supported = rz_has(raw,"isLeEnabled=true")   ? 1 : 0;
        char tmp[64];
        if (rz_extract_field(raw,"name",   tmp,sizeof(tmp))) strncpy(b->local_name,tmp,sizeof(b->local_name)-1);
        if (rz_extract_field(raw,"address",tmp,sizeof(tmp))) strncpy(b->local_mac, tmp,sizeof(b->local_mac)-1);
        b->backend    = g_backend;
        b->confidence = RZ_CONF_INFERRED;
        free(raw);
    }
    if (b->enabled == -1) {
        char *st = rz_prebaked_get("/sys/class/bluetooth/hci0/type");
        if (st) { b->enabled = 1; b->backend = RZ_BACKEND_SYSFS; b->confidence = RZ_CONF_FALLBACK; free(st); }
        else      b->enabled = 0;
    }
}

/* ── Layer 14: NFC ─────────────────────────────────────────────── */

void rz_probe_nfc(rz_nfc_t *n) {
    memset(n, 0, sizeof(*n));
    n->supported = n->enabled = n->tag_present = -1;
    n->confidence = RZ_CONF_ABSENT;
    strncpy(n->state, "UNKNOWN", sizeof(n->state)-1);
    char *raw = ds("nfc");
    if (raw) {
        n->supported    = 1;
        n->enabled      = (rz_has(raw,"mState=on")||rz_has(raw,"enabled=true")) ? 1 : 0;
        n->tag_present  = rz_has(raw,"tag=")       ? 1 : 0;
        n->beam_enabled = rz_has(raw,"beam=true")  ? 1 : 0;
        n->hce_active   = rz_has(raw,"hce=active") ? 1 : 0;
        strncpy(n->state, n->enabled ? "ON" : "OFF", sizeof(n->state)-1);
        n->backend    = g_backend;
        n->confidence = RZ_CONF_INFERRED;
        free(raw);
    } else { n->supported = sysfs_exists("/dev/nfc0") ? 1 : 0; }
}

/* ── Layer 15: Xiaomi Interference ────────────────────────────── */



void rz_probe_xiaomi(rz_xiaomi_t *x) {
    memset(x, 0, sizeof(*x));
    x->jx_policy_active = x->aml_conn_active = x->miui_wifi_active = -1;
    x->powerkeeper_throttling = x->turbosched_active = -1;
    x->perfshielder_active = x->miuibooster_active = -1;
    x->whetstone_power_active = x->smartpower_active = -1;
    x->confidence = RZ_CONF_ABSENT;

    char *svc = ds("activity");
    if (svc) {
        x->jx_policy_active   = rz_has(svc,"JXNetworkPolicyService") ? 1 : 0;
        x->aml_conn_active    = rz_has(svc,"AmlConnectivityService")  ? 1 : 0;
        x->miui_wifi_active   = rz_has(svc,"AmlMiuiWifiService")      ? 1 : 0;
        x->miuibooster_active = rz_has(svc,"MiuiBoosterService")      ? 1 : 0;
        free(svc);
    }
    char *v;
    if ((v = rz_prebaked_get("persist.sys.powerkeeper")))      { x->powerkeeper_throttling = strcmp(v,"1")==0 ? 1:0; free(v); }
    if ((v = rz_prebaked_get("persist.sys.miui.turbosched")))  { x->turbosched_active      = strcmp(v,"1")==0 ? 1:0; free(v); }
    if ((v = rz_prebaked_get("persist.sys.perfshielder")))     { x->perfshielder_active    = strcmp(v,"1")==0 ? 1:0; free(v); }
    if ((v = rz_prebaked_get("miui.whetstone.power")))         { x->whetstone_power_active = strlen(v)>0      ? 1:0; free(v); }
    if ((v = rz_prebaked_get("persist.sys.smartpower")))       { x->smartpower_active      = strcmp(v,"1")==0 ? 1:0; free(v); }

    char *route = rz_prebaked_get("/proc/net/route");
    if (route) {
        x->kernel_reports_connected = rz_has(route,"00000000") ? 1 : 0;
        char tmp[32];
        if (rz_extract_field(route,"Iface",tmp,sizeof(tmp)))
            strncpy(x->active_interface, tmp, sizeof(x->active_interface)-1);
        free(route);
    }
    char *conn = rz_prebaked_get("net.connectivity.status");
    x->aosp_reports_connected   = (conn && strcmp(conn,"1")==0) ? 1 : 0;
    if (conn) free(conn);
    x->xiaomi_reports_connected = (x->miui_wifi_active==1||x->aml_conn_active==1) ? 1 : 0;

    if (x->aosp_reports_connected != x->kernel_reports_connected ||
        x->xiaomi_reports_connected != x->kernel_reports_connected) {
        x->divergence_detected = 1;
        snprintf(x->divergence_reason, sizeof(x->divergence_reason),
                 "aosp=%d xiaomi=%d kernel=%d",
                 x->aosp_reports_connected,
                 x->xiaomi_reports_connected,
                 x->kernel_reports_connected);
    }
    x->backend    = g_backend;
    x->confidence = RZ_CONF_INFERRED;
}

/* ── Full Snapshot ─────────────────────────────────────────────── */

void rz_snapshot(rz_snapshot_t *out) {
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    out->timestamp = time(NULL);
    rz_probe_wifi      (&out->wifi);
    rz_probe_mobile    (&out->mobile);
    rz_probe_dns       (&out->dns);
    rz_probe_telephone (&out->telephone);
    rz_probe_sms       (&out->sms);
    rz_probe_mms       (&out->mms);
    rz_probe_ports     (&out->ports);
    rz_probe_ethernet  (&out->ethernet);
    rz_probe_uwb       (&out->uwb);
    rz_probe_fm        (&out->fm);
    rz_probe_usb       (&out->usb);
    rz_probe_infrared  (&out->infrared);
    rz_probe_bluetooth (&out->bluetooth);
    rz_probe_nfc       (&out->nfc);
    rz_probe_xiaomi    (&out->xiaomi);
    gettimeofday(&t1, NULL);
    out->poll_duration_ms = (int)((t1.tv_sec  - t0.tv_sec )*1000L +
                                  (t1.tv_usec - t0.tv_usec)/1000L);
}

/* ── State / Delta Engine ──────────────────────────────────────── */

void rz_state_update(rz_state_t *state) {
    state->prev = state->curr;
    rz_snapshot(&state->curr);
    if (state->first_poll) { state->first_poll = 0; return; }

    time_t now = state->curr.timestamp;

    if (state->prev.wifi.connected == 1 && state->curr.wifi.connected == 0) {
        state->wifi_flap_count++;
        state->wifi_connected_since = 0;
        rz_emit_delta(state, "wifi", "{\"event\":\"disconnected\"}");
    } else if (state->prev.wifi.connected == 0 && state->curr.wifi.connected == 1) {
        state->wifi_connected_since = now;
        rz_emit_delta(state, "wifi", "{\"event\":\"connected\"}");
    }
    if (state->prev.mobile.data_active != state->curr.mobile.data_active) {
        if (state->curr.mobile.data_active == 1) state->mobile_data_since = now;
        rz_emit_delta(state, "mobile", "{\"event\":\"data_state_change\"}");
    }
    if (state->prev.dns.resolves == 1 && state->curr.dns.resolves == 0) {
        state->dns_failing_since = now;
        gaveld_emit("rahzerd", "NO_DNS_RESOLUTION", 1.0, "host=dns.google");
        rz_emit_anomaly("dns_failure", "{\"host\":\"dns.google\"}");
    } else if (state->curr.dns.resolves == 1) {
        state->dns_failing_since = 0;
    }
    if (state->curr.dns.latency_ms > 2000) {
        state->dns_spike_count++;
        gaveld_emit("rahzerd", "DNS_ANOMALY", 1.0, "latency=high");
        rz_emit_anomaly("dns_latency_spike", "{\"latency_ms\":\"high\"}");
    }
    if (state->curr.xiaomi.divergence_detected && !state->prev.xiaomi.divergence_detected) {
        gaveld_emit("rahzerd", "XIAOMI_DIVERGENCE", 1.0, state->curr.xiaomi.divergence_reason);
        rz_emit_anomaly("xiaomi_divergence", state->curr.xiaomi.divergence_reason);
    }
    if (state->curr.ports.suspicious_listeners > state->prev.ports.suspicious_listeners) {
        gaveld_emit("rahzerd", "PORT_ANOMALY", 1.0, "suspicious_listener_detected");
        rz_emit_anomaly("suspicious_listener", "{\"layer\":\"ports\"}");
    }
}

/* ── Splinterd Emitters ────────────────────────────────────────── */

static int splinter_send(const char *msg) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_splinter, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    size_t len = strlen(msg);
    ssize_t w = write(fd, msg, len);
    close(fd);
    return (w == (ssize_t)len) ? 0 : -1;
}

int rz_emit_netstate(const rz_snapshot_t *snap) {
    char buf[EMIT_BUF];
    snprintf(buf, sizeof(buf),
        "APRIL|rahzerd|netstate|"
        "wifi=%d ssid=%.32s rssi=%d "
        "mobile=%d rat=%.16s roaming=%d "
        "dns=%d latency=%d "
        "bt=%d nfc=%d usb=%d "
        "ports_listen=%d suspicious=%d "
        "divergence=%d poll_ms=%d\n",
        snap->wifi.connected, snap->wifi.ssid, snap->wifi.rssi_dbm,
        snap->mobile.data_active, snap->mobile.rat_type, snap->mobile.roaming,
        snap->dns.resolves, snap->dns.latency_ms,
        snap->bluetooth.enabled, snap->nfc.enabled, snap->usb.connected,
        snap->ports.listen_count, snap->ports.suspicious_listeners,
        snap->xiaomi.divergence_detected, snap->poll_duration_ms);
        /* gaveld — network state signals */
    if (snap->mobile.roaming == 1)
        gaveld_emit("rahzerd", "ROAMING_DATA_ACTIVE", 1.0, "");
    if (snap->mobile.dual_sim == 1)
        gaveld_emit("rahzerd", "DUAL_SIM_ACTIVE", 1.0, "");
    if (snap->dns.private_dns_active == 0)
        gaveld_emit("rahzerd", "PRIVATE_DNS_INACTIVE", 1.0, "");
    if (snap->ports.listen_count > 20)
        gaveld_emit("rahzerd", "EXCESSIVE_CONNECTIONS", (double)snap->ports.listen_count, "");
    return splinter_send(buf);
}

int rz_emit_delta(const rz_state_t *state, const char *layer,
                  const char *detail_json) {
    char buf[EMIT_BUF];
    snprintf(buf, sizeof(buf), "APRIL|rahzerd|delta|layer=%s detail=%s\n",
             layer, detail_json);
    (void)state;
    return splinter_send(buf);
}

int rz_emit_anomaly(const char *type, const char *detail_json) {
    char buf[EMIT_BUF];
    snprintf(buf, sizeof(buf), "APRIL|rahzerd|anomaly|type=%s detail=%s\n",
             type, detail_json);
    return splinter_send(buf);
}

int rz_emit_lifecycle(const char *event_type) {
    char buf[256];
    snprintf(buf, sizeof(buf), "APRIL|rahzerd|%s|poll_sec=%d backend=%s\n",
             event_type, g_poll_sec, RZ_BACKEND_NAMES[g_backend]);
    return splinter_send(buf);
}

/* ── Main ──────────────────────────────────────────────────────── */


/* ── JSON + Baseline additions ─────────────────────────────────── */
static char g_baseline_bssid[18] = {0};
static int  g_baseline_dns_ms   = 0;
static int  g_baseline_established = 0;

static void rz_write_json(const rz_snapshot_t *snap) {
    FILE *f = fopen(RESULTS_FILE, "w");
    if (!f) return;
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", localtime(&t));

    fprintf(f,
        "{\n"
        "  \"daemon\": \"rahzerd\",\n"
        "  \"version\": \"2.0\",\n"
        "  \"timestamp\": \"%s\",\n"
        "  \"poll_interval_sec\": %d,\n"
        "  \"wifi\": {\n"
        "    \"connected\": %d,\n"
        "    \"ssid\": \"%s\",\n"
        "    \"bssid\": \"%s\",\n"
        "    \"rssi_dbm\": %d,\n"
        "    \"tx_bytes\": %ld,\n"
        "    \"rx_bytes\": %ld\n"
        "  },\n"
        "  \"mobile\": {\n"
        "    \"rat\": \"%s\",\n"
        "    \"roaming\": %d,\n"
        "    \"signal_dbm\": %d,\n"
        "    \"data_active\": %d\n"
        "  },\n"
        "  \"dns\": {\n"
        "    \"resolves\": %d,\n"
        "    \"latency_ms\": %d,\n"
        "    \"private_dns\": %d\n"
        "  },\n"
        "  \"ports\": {\n"
        "    \"established_tcp4\": %d,\n"
        "    \"established_tcp6\": %d,\n"
        "    \"suspicious_listeners\": %d\n"
        "  },\n"
        "  \"baseline\": {\n"
        "    \"established\": %s,\n"
        "    \"wifi_bssid\": \"%s\",\n"
        "    \"dns_latency_ms\": %d\n"
        "  },\n"
        "  \"xiaomi_divergence\": %d,\n"
        "  \"poll_duration_ms\": %d\n"
        "}\n",
        ts, g_poll_sec,
        snap->wifi.connected, snap->wifi.ssid, snap->wifi.bssid,
        snap->wifi.rssi_dbm, snap->wifi.tx_bytes, snap->wifi.rx_bytes,
        snap->mobile.rat_type, snap->mobile.roaming,
        snap->mobile.signal_dbm, snap->mobile.data_active,
        snap->dns.resolves, snap->dns.latency_ms, snap->dns.private_dns_active,
        snap->ports.established_tcp4, snap->ports.established_tcp6,
        snap->ports.suspicious_listeners,
        g_baseline_established ? "true" : "false",
        g_baseline_bssid, g_baseline_dns_ms,
        snap->xiaomi.divergence_detected,
        snap->poll_duration_ms);
    fflush(f);
    fclose(f);
}

static void rz_establish_baseline(const rz_snapshot_t *snap) {
    if (g_baseline_established) return;
    if (!snap->wifi.connected) return;
    strncpy(g_baseline_bssid, snap->wifi.bssid, sizeof(g_baseline_bssid)-1);
    g_baseline_dns_ms = snap->dns.latency_ms;
    g_baseline_established = 1;
    fprintf(stderr, "[RAHZERD] Baseline established: bssid=%s dns=%dms\n",
            g_baseline_bssid, g_baseline_dns_ms);
}

/* Non-blocking exec with FD isolation — replaces system() */
static void rz_exec_dump(void) {
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        /* Child: close all inherited FDs > 2 */
        for (int fd = 3; fd < 256; fd++) close(fd);
        alarm(12);
        execl("/data/data/com.termux/files/usr/bin/bash", "bash", "-c",
              "/data/data/com.termux/files/home/MiuiserPeruser/scripts/dump_rahzerd.sh",
              NULL);
        _exit(127);
    }
    /* Parent: wait with timeout */
    int status;
    time_t start = time(NULL);
    while (time(NULL) - start < 13) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        usleep(100000);
    }
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}


int main(void) {
    const char *env;
    if ((env = getenv("RAHZERD_POLL_SEC"))) {
        int v = atoi(env); g_poll_sec = (v >= 5) ? v : DEFAULT_POLL_SEC;
    }
    if ((env = getenv("RAHZERD_SPLINTER"))) g_splinter = env;
    if ((env = getenv("RAHZERD_DEBUG")))    g_debug    = atoi(env);
    if ((env = getenv("RAHZERD_DNS_HOST"))) g_dns_host = env;
    if ((env = getenv("RAHZERD_LOG_PATH"))) {
        g_log_fp = fopen(env, "a");
        if (!g_log_fp) rzlog("WARN", "cannot open log: %s", env);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sig;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    rzlog("INFO", "rahzerd v2 start poll_sec=%d", g_poll_sec);
    g_backend = rz_detect_backend();
    rz_emit_lifecycle("online");

    FILE *pf = fopen(BASE "/pipes/pids/rahzerd.pid", "w");
    if (pf) { fprintf(pf, "%d\n", getpid()); fclose(pf); }

    rz_state_t state;
    memset(&state, 0, sizeof(state));
    state.first_poll = 1;

    while (g_rahzerd_running) {
        /* Pre-bake all probe data in one rish session */
        rz_exec_dump();
        rz_load_probe_data();
        rz_state_update(&state);
        rz_establish_baseline(&state.curr);
        rz_write_json(&state.curr);
        if (!state.first_poll)
            rz_emit_netstate(&state.curr);
        rzlog("INFO",
              "poll#%d wifi=%s mobile=%s dns=%s(%dms) bt=%s ports=%d div=%d dur=%dms",
              state.curr.poll_cycle,
              state.curr.wifi.connected == 1 ? "connected" : "disconnected",
              state.curr.mobile.data_active == 1 ? "active" : "inactive",
              state.curr.dns.resolves == 1 ? "ok" : "fail",
              state.curr.dns.latency_ms,
              state.curr.bluetooth.enabled == 1 ? "on" : "off",
              state.curr.ports.listen_count,
              state.curr.xiaomi.divergence_detected,
              state.curr.poll_duration_ms);
        state.curr.poll_cycle++;
        for (int i = 0; i < g_poll_sec && g_rahzerd_running; i++)
            sleep(1);
    }

    rz_emit_lifecycle("offline");
    rzlog("INFO", "rahzerd shutdown");
    unlink(BASE "/pipes/pids/rahzerd.pid");
    if (g_log_fp) fclose(g_log_fp);
    return 0;
}
