/*
 * burned.c — MIUI / HyperOS Policy & Optimisation Guardian
 *
 * Every poll:
 *   - Read 16 MIUI/HyperOS system properties
 *   - Detect CHANGES since last poll and highlight them
 *   - Check MIUI telemetry consent (MSA), memory extension, PowerKeeper
 *   - Check background process limit and cleaner aggressiveness
 *   - Report to turtlecomd hub (fire-and-forget, tolerant of hub downtime)
 *   - Emit APRIL events when invasive properties are active
 *
 * APRIL events emitted:
 *   miui_policy  — invasive policy active (telemetry, restriction, turbo)
 *   miui_change  — a monitored property changed value
 */

#include "ipc_globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#define DAEMON_NAME  "burned"
#define POLL_SEC     60
#define BUF_SIZE     2048
#define MAX_PROPS    20

/* ── Splinterd emit ───────────────────────────────────────────────────── */

static void splinterd_emit(const char *type, const char *payload)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        char buf[512];
        int n = snprintf(buf, sizeof(buf),
                         "APRIL|" DAEMON_NAME "|%s|%s\n", type, payload);
        if (n > 0) write(fd, buf, (size_t)n);
    }
    close(fd);
}

/* ── Turtlecomd send (non-blocking, tolerant of hub absence) ──────────── */

static void hub_report(const char *msg)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TURTLE_SOCKET, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        write(fd, msg, strlen(msg));
    close(fd);
}

/* ── Getprop helper ───────────────────────────────────────────────────── */

static void getprop(const char *key, char *out, size_t outlen)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "getprop %s 2>/dev/null", key);
    FILE *f = popen(cmd, "r");
    if (!f) { strncpy(out, "(err)", outlen); return; }
    out[0] = '\0';
    fgets(out, (int)outlen, f);
    pclose(f);
    out[strcspn(out, "\n\r")] = '\0';
    if (!out[0]) strncpy(out, "(unset)", outlen);
}

/* ── Property table ───────────────────────────────────────────────────── */

typedef struct {
    const char *key;
    const char *label;
    char        cur[64];
    char        prev[64];
    int         invasive_if_1;  /* flag APRIL event if value == "1" */
} prop_t;

static prop_t g_props[MAX_PROPS] = {
    { "persist.sys.miui_optimization",      "MIUI Optimisation",   {0},{0}, 0 },
    { "persist.sys.background_data",        "Background Data",     {0},{0}, 0 },
    { "persist.sys.powerkeeper",            "PowerKeeper",         {0},{0}, 1 },
    { "persist.sys.autostart",              "Autostart Control",   {0},{0}, 0 },
    { "persist.sys.miui_restricted_mode",   "Restricted Mode",     {0},{0}, 1 },
    { "persist.sys.memory_extension_enabled","RAM Extension",      {0},{0}, 0 },
    { "persist.sys.performance_mode",       "Performance Mode",    {0},{0}, 0 },
    { "persist.sys.game_turbo_enabled",     "Game Turbo",          {0},{0}, 1 },
    { "persist.sys.cleaner_level",          "Cleaner Level",       {0},{0}, 0 },
    { "persist.sys.miui.msa.consent",       "MSA Telemetry",       {0},{0}, 1 },
    { "persist.sys.blocked_pkgs",           "Blocked Packages",    {0},{0}, 0 },
    { "persist.miui.region",                "MIUI Region",         {0},{0}, 0 },
    { "ro.miui.ui.version.name",            "MIUI Version",        {0},{0}, 0 },
    { "persist.sys.miui.turbosched",        "TurboSched",          {0},{0}, 0 },
    { "persist.sys.perfshielder",           "PerfShielder",        {0},{0}, 0 },
    { "miui.whetstone.power",               "WhetstoneMode",       {0},{0}, 0 },
};
static const int g_nprops = 16;
static int       g_first  = 1;

/* ── Poll ─────────────────────────────────────────────────────────────── */

static void poll_policy(void)
{
    char ts[32];
    time_t t = time(NULL);
    strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&t));

    printf("\n[BURNED] ── MIUI/HyperOS Policy Scan  %s ──────────────────\n", ts);
    printf("[BURNED]  %-28s  %-20s  %s\n",
           "Property", "Value", "Status");
    printf("[BURNED]  ──────────────────────────────────────────────────────\n");

    int invasive_count = 0;
    int change_count   = 0;
    char invasive_list[256] = {0};
    char change_list[256]   = {0};

    for (int i = 0; i < g_nprops; i++) {
        prop_t *p = &g_props[i];
        /* Save previous */
        memcpy(p->prev, p->cur, sizeof(p->cur));
        /* Read current */
        getprop(p->key, p->cur, sizeof(p->cur));

        /* Status flags */
        const char *changed = (!g_first && strcmp(p->cur, p->prev) != 0) ? " CHANGED" : "";
        const char *invasive = (p->invasive_if_1 && strcmp(p->cur, "1") == 0) ? " [!]" : "";

        printf("[BURNED]  %-28s  %-20s%s%s\n",
               p->label, p->cur, invasive, changed);

        if (!g_first && strcmp(p->cur, p->prev) != 0) {
            change_count++;
            if (change_list[0]) strncat(change_list, ",", sizeof(change_list)-strlen(change_list)-1);
            strncat(change_list, p->key, sizeof(change_list)-strlen(change_list)-1);
        }
        if (p->invasive_if_1 && strcmp(p->cur, "1") == 0) {
            invasive_count++;
            if (invasive_list[0]) strncat(invasive_list, ",", sizeof(invasive_list)-strlen(invasive_list)-1);
            strncat(invasive_list, p->label, sizeof(invasive_list)-strlen(invasive_list)-1);
        }
    }

    printf("[BURNED]  ──────────────────────────────────────────────────────\n");
    printf("[BURNED]  Invasive flags: %-3d  Changes this poll: %d\n",
           invasive_count, change_count);

    /* Hub report */
    char hub_msg[BUF_SIZE];
    snprintf(hub_msg, sizeof(hub_msg),
             "STATUS BURNED invasive=%d changes=%d region=%s version=%s\n",
             invasive_count, change_count,
             g_props[11].cur,   /* persist.miui.region */
             g_props[12].cur);  /* ro.miui.ui.version.name */
    hub_report(hub_msg);

    /* APRIL: invasive policy */
    if (invasive_count > 0) {
        char ev[512];
        snprintf(ev, sizeof(ev),
                 "count=%d active=%.180s", invasive_count, invasive_list);
        splinterd_emit("miui_policy", ev);
    }

    /* APRIL: property changed */
    if (change_count > 0) {
        char ev[512];
        snprintf(ev, sizeof(ev),
                 "count=%d props=%.220s", change_count, change_list);
        splinterd_emit("miui_change", ev);
    }

    g_first = 0;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("[BURNED] MIUI/HyperOS Policy Guardian: ONLINE\n");
    printf("[BURNED] Monitoring %d system properties — poll every %ds\n",
           g_nprops, POLL_SEC);

    for (;;) {
        poll_policy();
        sleep(POLL_SEC);
    }

    return 0;
}
