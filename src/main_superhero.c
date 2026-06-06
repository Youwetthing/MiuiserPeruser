#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "superhero_mode.h"
#include "splinter_dojo.h"
#include "leo_detection.h"

#define CONSENT_LOCK "/data/data/com.termux/files/home/MiuiserPeruser/data/.sensei_consent.lock"
#define DB_PATH      "/data/data/com.termux/files/home/MiuiserPeruser/data/sensei_dojo.db"
#define DATA_DIR     "/data/data/com.termux/files/home/MiuiserPeruser/data"

extern void april_log(const char* level, const char* format, ...);

static int consent_granted(void) {
    FILE *f = fopen(CONSENT_LOCK, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

static void write_consent_lock(const char *depth) {
    /* Ensure data dir exists */
    system("mkdir -p " DATA_DIR);
    FILE *f = fopen(CONSENT_LOCK, "w");
    if (f) { fprintf(f, "%s\n", depth); fclose(f); }
}

static void purge_data(void) {
    printf("\n  Purging all Superhero Mode data...\n");
    remove(DB_PATH);
    printf("  \u2705 sensei_dojo.db removed.\n  Done.\n\n");
}

static void print_banner(void) {
    printf("\n");
    printf("\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n");
    printf("\u2551           MIUISERPERUSER \u2014 SUPERHERO MODE                       \u2551\n");
    printf("\u2551           Deep Device Intelligence Scanner                      \u2551\n");
    printf("\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n\n");
}

static const char *run_consent_gate(void) {
    print_banner();
    printf("  Before this tool runs, you need to understand what it does.\n\n");
    printf("  WHAT GETS READ FROM YOUR DEVICE:\n\n");
    printf("  [SURFACE]\n");
    printf("    \u2022 Running process list and CPU/memory usage\n");
    printf("    \u2022 Foreground app and recent activity\n");
    printf("    \u2022 Battery state and wakelock activity\n\n");
    printf("  [STANDARD] \u2014 everything above, plus:\n");
    printf("    \u2022 All active network sockets (/proc/net/)\n");
    printf("    \u2022 MIUI/HyperOS system properties and telemetry flags\n");
    printf("    \u2022 Installed app permissions and appops settings\n");
    printf("    \u2022 System binary presence checks\n\n");
    printf("  [DEEP] \u2014 everything above, plus:\n");
    printf("    \u2022 Kernel memory maps and loaded modules\n");
    printf("    \u2022 Syscall table and IDT integrity checks\n");
    printf("    \u2022 Hook detection in running process memory\n");
    printf("    \u2022 Per-PID memory and behaviour analysis\n");
    printf("    \u2022 Input surface audit (a11y, IME, overlays)\n");
    printf("    \u2022 Full device baseline fingerprint (stored locally)\n\n");
    printf("  WHAT GETS STORED:\n");
    printf("    \u2022 All results stay ON THIS DEVICE ONLY\n");
    printf("    \u2022 Nothing is transmitted anywhere\n");
    printf("    \u2022 Stored in: data/sensei_dojo.db\n");
    printf("    \u2022 Purge anytime with: superhero --purge\n\n");
    printf("  REQUIREMENTS:\n");
    printf("    \u2022 DEEP scan requires rish/ADB elevated access\n");
    printf("    \u2022 This tool is for personal device security research only\n\n");
    printf("\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\n\n");

    printf("  Type  I CONSENT  to continue: ");
    fflush(stdout);
    char input[64] = {0};
    if (!fgets(input, sizeof(input), stdin)) exit(0);
    input[strcspn(input, "\n")] = 0;
    if (strcmp(input, "I CONSENT") != 0) {
        printf("\n  Consent not given. Exiting.\n\n");
        exit(0);
    }

    printf("\n  Select scan depth:\n");
    printf("    [1] surface  \u2014 process + battery + activity (no kernel reads)\n");
    printf("    [2] standard \u2014 + network + MIUI props + appops\n");
    printf("    [3] deep     \u2014 + kernel maps + hooks + full baseline\n\n");
    printf("  Choice (1/2/3): ");
    fflush(stdout);
    char choice[8] = {0};
    if (!fgets(choice, sizeof(choice), stdin)) return "standard";
    choice[strcspn(choice, "\n")] = 0;

    const char *depth = "standard";
    if (strcmp(choice, "1") == 0)      depth = "surface";
    else if (strcmp(choice, "3") == 0) depth = "deep";

    if (strcmp(depth, "deep") == 0) {
        printf("\n  \u26a0\ufe0f  DEEP scan reads kernel memory maps and syscall tables.\n");
        printf("  This is powerful and irreversible per session.\n");
        printf("  Type  DEEP CONFIRMED  to proceed: ");
        fflush(stdout);
        char confirm[32] = {0};
        if (!fgets(confirm, sizeof(confirm), stdin)) exit(0);
        confirm[strcspn(confirm, "\n")] = 0;
        if (strcmp(confirm, "DEEP CONFIRMED") != 0) {
            printf("\n  Deep scan cancelled. Running standard scan instead.\n");
            depth = "standard";
        }
    }

    write_consent_lock(depth);
    printf("\n  \u2705 Consent recorded. Scan depth: %s\n\n", depth);
    return depth;
}

static const char *get_saved_depth(void) {
    FILE *f = fopen(CONSENT_LOCK, "r");
    if (!f) return "standard";
    static char depth[32] = "standard";
    if (fgets(depth, sizeof(depth), f)) {
        depth[strcspn(depth, "\n")] = 0;
    }
    fclose(f);
    return depth;
}

int main(int argc, char **argv) {
    /* Handle --purge */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--purge") == 0) {
            purge_data();
            return 0;
        }
    }

    const char *scan_depth = "standard";

    /* Check for depth override flags */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--surface")  == 0) { scan_depth = "surface";  break; }
        if (strcmp(argv[i], "--standard") == 0) { scan_depth = "standard"; break; }
        if (strcmp(argv[i], "--deep")     == 0) { scan_depth = "deep";     break; }
    }

    /* Consent gate — only shown once ever */
    if (!consent_granted()) {
        scan_depth = run_consent_gate();
    } else {
        /* Returning user — use saved depth unless overridden by flag */
        int flag_set = 0;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--surface")  == 0 ||
                strcmp(argv[i], "--standard") == 0 ||
                strcmp(argv[i], "--deep")     == 0) {
                flag_set = 1; break;
            }
        }
        if (!flag_set) scan_depth = get_saved_depth();
    }

    /* Initialise */
    april_log("INFO", "Superhero Mode: one-and-done scan selected.");
    splinter_set_depth(scan_depth);
    splinter_init();
    leo_init();

    /* Run scan */
    splinter_run_scan_cycle(0);

    /* Collect and display metrics */
    splinter_metrics_t metrics = {0};
    splinter_collect_metrics(&metrics);
    april_log("INFO", "Superhero Mode: backend in use = %s",
              backend_name(splinter_get_backend()));
    printf("[thermal] %d\n[battery] %d\n[cpu_freq] %d\n",
           metrics.thermal, metrics.battery, metrics.cpu_freq);
    april_log("INFO", "Action: thermal=%d", metrics.thermal);
    april_log("INFO", "Action: battery=%d", metrics.battery);
    april_log("INFO", "Action: cpu_freq=%d", metrics.cpu_freq);

    printf("+------------------+---------+\n");
    printf("| Metric         | Value   |\n");
    printf("+------------------+---------+\n");
    printf("| Thermal (mC)   | %-7d |\n", metrics.thermal);
    printf("| Battery (%%)    | %-7d |\n", metrics.battery);
    printf("| CPU freq (kHz) | %-7d |\n", metrics.cpu_freq);
    printf("+------------------+---------+\n");

    splinter_shutdown();
    return 0;
}
