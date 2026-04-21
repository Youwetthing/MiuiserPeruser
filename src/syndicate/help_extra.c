#include <stdio.h>

/* Print extra help content: GitHub link and short internals explanation.
   Call print_help_extra() from the existing help output in miuiserperuser.c. */
void print_help_extra(void) {
    puts("");
    puts("Shizuku GitHub: https://github.com/RikkaApps/Shizuku");
    puts("");
    puts("How Shizuku works (short):");
    puts("  Shizuku runs a privileged Java process (often started via app_process) and");
    puts("  exposes selected system APIs over Binder so apps can call privileged APIs");
    puts("  without full root. It supports three startup modes: root, ADB (USB), and");
    puts("  wireless pairing. Different modes expose different runtime artifacts, so");
    puts("  detection uses multiple probes (package, binder/service list, process).");
    puts("");
    puts("If Shizuku is present but not responding, try starting it via the app or");
    puts("the ADB start flow, then re-run the daemon selftest.");
    puts("");
}
