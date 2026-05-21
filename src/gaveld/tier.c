#define _GNU_SOURCE
#include "tier.h"
#include "config.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* ── Own daemon list ─────────────────────────────────────────────────────── */

static const char *OWN_DAEMONS[] = {
    /* Syndicate C daemons */
    "splinterd", "krangd", "turtlecomd", "rahzerd",
    "leatherheadd", "metalheadd", "granitord", "ratkingd",
    "shredderd", "fugitoidd", "bebopd", "burned", "rocksteadyd",
    "tigerclawd",
    /* Legacy daemons */
    "connectivityd", "networkd", "foot_clan_supreme",
    "foot_portwatchd", "foot_resurrectord", "foot_ipcshadowd",
    "footrunner", "cpud", "processd", "storaged", "thermald",
    "sysportd", "daemonhunterd", "miuiserperuser",
    "miuiserperuser-daemon", "miuid", "brain-ctl",
    /* Judicial layer */
    "april_o_neil", "court_orchestrator", "court_core_engine",
    "judge_executor", "parole_engine", "scoring_engine",
    "scored", "gaveld", "internal_affairs", "consent_gate",
    "escalation_daemon", "visitors_pass_daemon",
    "turtlepower_daemon", "superhero_adapter", "superhero",
    "baxter_stockman", "court_dispatcher",
    NULL
};

/* ── MIUI / AOSP package prefixes ────────────────────────────────────────── */

static const char *SYSTEM_PREFIXES[] = {
    "com.miui.", "com.xiaomi.", "com.hyperos.", "android.",
    "com.android.", "miui.", "com.lbe.", "com.qualcomm.",
    "com.qti.", "org.codeaurora.",
    NULL
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Strip path — /data/.../rahzerd → rahzerd */
static const char *basename_of(const char *source) {
    const char *p = strrchr(source, '/');
    return p ? p + 1 : source;
}

int tier_is_own_daemon(const char *source) {
    const char *base = basename_of(source);
    for (int i = 0; OWN_DAEMONS[i]; i++)
        if (strcmp(base, OWN_DAEMONS[i]) == 0) return 1;
    return 0;
}

int tier_is_sovereignty(const char *source) {
    FILE *fp = fopen(SOVEREIGNTY_LIST, "r");
    if (!fp) return 0;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* Format: package.name[|optional_note] */
        char pkg[256] = {0};
        sscanf(line, "%255[^|\n]", pkg);
        /* Trim trailing whitespace */
        int len = (int)strlen(pkg);
        while (len > 0 && (pkg[len-1] == ' ' || pkg[len-1] == '\t')) pkg[--len] = '\0';
        if (pkg[0] && strcmp(pkg, source) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

int tier_is_system(const char *source) {
    for (int i = 0; SYSTEM_PREFIXES[i]; i++)
        if (strncmp(source, SYSTEM_PREFIXES[i],
                    strlen(SYSTEM_PREFIXES[i])) == 0) return 1;
    return 0;
}

/* ── Main entry point ────────────────────────────────────────────────────── */

double tier_modifier(const char *source) {
    if (!source || !*source) return TIER_MOD_UNKNOWN;

    if (tier_is_own_daemon(source)) {
        glog("DEBUG", "tier=OWN src=%s mod=%.2f", source, TIER_MOD_OWN_DAEMON);
        return TIER_MOD_OWN_DAEMON;
    }
    if (tier_is_sovereignty(source)) {
        glog("DEBUG", "tier=SOV src=%s mod=%.2f", source, TIER_MOD_SOVEREIGNTY);
        return TIER_MOD_SOVEREIGNTY;
    }
    if (tier_is_system(source)) {
        glog("DEBUG", "tier=SYS src=%s mod=%.2f", source, TIER_MOD_MIUI_AOSP);
        return TIER_MOD_MIUI_AOSP;
    }

    glog("DEBUG", "tier=UNK src=%s mod=%.2f", source, TIER_MOD_UNKNOWN);
    return TIER_MOD_UNKNOWN;
}
