#include "splinter_dojo.h"
#include "backend_common.h"
#include "leo_detection.h"
#include "../core/include/april_event.h"
#include "../core/include/sensei_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

extern void april_log(const char* level, const char* format, ...);
extern SENSEI_STATUS april_detection_list_append(SENSEI_DETECTION_LIST *list, const SENSEI_DETECTION *det);
extern SENSEI_STATUS casey_hook_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS casey_kernel_check(SENSEI_DETECTION_LIST *results);
extern ScanResult    scan_casey_input(void);
extern SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *results);
extern void          raph_memory_scan(uint32_t pid, SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_integrity_check(SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_behavior_analyze(uint32_t pid, SENSEI_DETECTION_LIST *results);
extern SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *results);

static BACKEND_TYPE g_backend = BACKEND_NONE;
static char g_scan_depth[32] = "standard";

void splinter_set_depth(const char *depth) {
    if (depth) strncpy(g_scan_depth, depth, sizeof(g_scan_depth) - 1);
}

static char *rish_cmd(const char *cmd) {
    char full[2048];
    snprintf(full, sizeof(full),
        "RISH_APPLICATION_ID=com.termux "
        "/data/data/com.termux/files/home/Rish/rish -c '%s' 2>/dev/null", cmd);
    FILE *fp = popen(full, "r");
    if (!fp) return NULL;
    size_t sz = 8192; char *out = malloc(sz);
    if (!out) { pclose(fp); return NULL; }
    out[0] = 0; size_t pos = 0; char buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (pos + len + 1 >= sz) { sz *= 2; char *t = realloc(out, sz); if (!t) break; out = t; }
        memcpy(out + pos, buf, len); pos += len;
    }
    out[pos] = 0; pclose(fp); return out;
}

SENSEI_STATUS splinter_init(void) {
    april_log("INFO", "Splinter Dojo: initializing backend...");
    g_backend = backend_strategy_select();
    april_emit_event(APRIL_EVENT_BACKEND_SELECTED, (int)g_backend);
    leo_init();
    return SENSEI_STATUS_OK;
}

#define FINDINGS_LOG "/data/data/com.termux/files/home/MiuiserPeruser/data/last_scan.json"
static FILE *g_findings_log = NULL;
static int   g_findings_count = 0;

static void print_findings(const char *turtle, SENSEI_DETECTION_LIST *r) {
    if (!r || r->count == 0) {
        april_log("INFO", "%s: clean", turtle);
        return;
    }
    april_log("INFO", "%s: %u finding(s)", turtle, r->count);
    SENSEI_DETECTION *d = r->head;
    while (d) {
        const char *pri = "LOW";
        if (d->priority == SENSEI_EVENT_PRIORITY_MEDIUM)   pri = "MEDIUM";
        if (d->priority == SENSEI_EVENT_PRIORITY_HIGH)     pri = "HIGH";
        if (d->priority == SENSEI_EVENT_PRIORITY_CRITICAL) pri = "CRITICAL";
        printf("  [%s][%s] %s\n", pri, d->detection_type, d->description);
        /* Write to JSON log */
        if (g_findings_log) {
            if (g_findings_count > 0) fprintf(g_findings_log, ",\n");
            fprintf(g_findings_log,
                "  {\"turtle\":\"%s\",\"priority\":\"%s\","
                "\"type\":\"%s\",\"description\":\"%s\"}",
                turtle, pri, d->detection_type, d->description);
            g_findings_count++;
        }
        d = d->next;
    }
}

static void free_results(SENSEI_DETECTION_LIST *r) {
    SENSEI_DETECTION *cur = r->head;
    while (cur) { SENSEI_DETECTION *n = cur->next; free(cur); cur = n; }
    memset(r, 0, sizeof(*r));
}

SENSEI_STATUS splinter_run_scan_cycle(uint32_t interval_ms) {
    (void)interval_ms;
    int deep     = (strcmp(g_scan_depth, "deep")     == 0);
    int standard = deep || (strcmp(g_scan_depth, "standard") == 0);

    april_log("INFO", "Splinter Dojo: starting full scan cycle (depth=%s)", g_scan_depth);
    g_findings_log = fopen(FINDINGS_LOG, "w");
    g_findings_count = 0;
    if (g_findings_log) fprintf(g_findings_log, "[\n");
    april_emit_event(APRIL_EVENT_SCAN_START, 0);
    SENSEI_DETECTION_LIST results = {0};

    /* ── SURFACE ── */
    printf("\n[SPLINTER] \u2500\u2500 Surface Scan \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");
    leo_full_scan();

    /* ── STANDARD ── */
    if (standard) {
        printf("\n[SPLINTER] \u2500\u2500 Standard Scan \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");

        printf("[RAPH] Network audit...\n");
        raph_network_scan(&results);
        print_findings("RAPH_NET", &results);
        free_results(&results);

        printf("[MIKEY] HyperOS/MIUI audit...\n");
        mikey_miui(&results);
        print_findings("MIKEY", &results);
        free_results(&results);

        printf("[DON] File integrity...\n");
        don_integrity_check(&results);
        print_findings("DON_INTEGRITY", &results);
        free_results(&results);

        printf("[DON] Memory pressure...\n");
        don_memorypressure_check(&results);
        print_findings("DON_MEM", &results);
        free_results(&results);
    }

    /* ── DEEP ── */
    if (deep) {
        printf("\n[SPLINTER] \u2500\u2500 Deep Scan \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\n");

        printf("[CASEY] Hook & instrumentation scan...\n");
        casey_hook_check(&results);
        print_findings("CASEY_HOOK", &results);
        free_results(&results);

        printf("[CASEY] Kernel integrity scan...\n");
        casey_kernel_check(&results);
        print_findings("CASEY_KERNEL", &results);
        free_results(&results);

        printf("[CASEY] Input surface scan...\n");
        ScanResult cr = scan_casey_input();
        if (cr.threat_level > 0 || cr.anomaly_score > 0) {
            printf("  [CASEY_INPUT] score=%.0f threat=%d %s\n",
                   cr.anomaly_score, cr.threat_level, cr.report);
        } else {
            printf("  [CASEY_INPUT] clean\n");
        }

        /* Verbose per-PID scan */
        printf("\n[RAPH/DON] Per-process deep scan...\n");
        /* Dump all PIDs via script — avoids rish quoting/truncation */
        system("/data/data/com.termux/files/home/MiuiserPeruser/scripts/dump_pids.sh");
        FILE *pid_fp = fopen("/data/data/com.termux/files/home/MiuiserPeruser/pipes/state/pid_names", "r");
        char pid_buf[65536] = {0};
        if (pid_fp) { fread(pid_buf, 1, sizeof(pid_buf)-1, pid_fp); fclose(pid_fp); }
        char *pid_names = pid_buf;
        char *pids = pid_names;
        if (pids) {
            char *tok = strtok(pids, "\n");
            while (tok) {
                char *colon = strchr(tok, ':');
                if (!colon) { tok = strtok(NULL, "\n"); continue; }
                *colon = 0;
                uint32_t pid = (uint32_t)atoi(tok);
                char name[32] = "?";
                strncpy(name, colon+1, sizeof(name)-1);
                name[strcspn(name, "\n\r")] = 0;
                if (pid > 1) {
                    printf("  \u2192 [%5u] %-20s ", pid, name);
                    fflush(stdout);

                    raph_memory_scan(pid, &results);
                    /* don_behavior_analyze(pid, &results); -- disabled: rish per-PID too slow */

                    if (results.count > 0) {
                        printf("\u26a0\ufe0f  %u finding(s)\n", results.count);
                        print_findings("    ", &results);
                    } else {
                        printf("\u2714\ufe0f\n");
                    }
                    free_results(&results);
                }
                tok = strtok(NULL, "\n");
            }
        }
    }

    april_emit_event(APRIL_EVENT_SCAN_END, 0);
    if (g_findings_log) { fprintf(g_findings_log, "\n]\n"); fclose(g_findings_log); g_findings_log = NULL; }
    april_log("INFO", "Splinter Dojo: scan cycle complete");
    return SENSEI_STATUS_OK;
}

SENSEI_STATUS splinter_collect_metrics(splinter_metrics_t *out) {
    if (!out) return SENSEI_STATUS_ERROR;

    char *b = rish_cmd("dumpsys battery 2>/dev/null | grep level:");
    out->battery = -1;
    if (b) { char *l = strstr(b, "level:"); if (l) out->battery = atoi(l+6); free(b); }

    char *t = rish_cmd(
        "dumpsys thermalservice 2>/dev/null | grep -m1 'mName=CPU' | grep -oE 'mValue=[0-9.]+' | cut -d= -f2");
    out->thermal = -1;
    if (t && strlen(t) > 0) { out->thermal = (int)(atof(t)*1000); free(t); } else { free(t); }

    char *c = rish_cmd(
        "cat /sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq 2>/dev/null");
    out->cpu_freq = -1;
    if (c && strlen(c) > 0) { out->cpu_freq = atoi(c); free(c); } else { free(c); }

    april_emit_event(APRIL_EVENT_METRIC_THERMAL,  out->thermal);
    april_emit_event(APRIL_EVENT_METRIC_BATTERY,  out->battery);
    april_emit_event(APRIL_EVENT_METRIC_CPUFREQ,  out->cpu_freq);
    return SENSEI_STATUS_OK;
}

void splinter_shutdown(void) {
    leo_shutdown();
    april_log("INFO", "Splinter Dojo: shutdown.");
}

BACKEND_TYPE splinter_get_backend(void) { return g_backend; }
