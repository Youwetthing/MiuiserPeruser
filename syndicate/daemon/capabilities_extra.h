#ifndef CAPABILITIES_EXTRA_H
#define CAPABILITIES_EXTRA_H

/* Core capability state */
struct capabilities_state {
    int adb;
    int shizuku;
    int proc_access;
    int sys_access;
    int miui_services;

    /* New: Sensei port bridge capabilities */
    int port_bridge_available;
    int port_bridge_info_ok;
};

/* Existing functions */
void detect_capabilities(void);
void capability_print_summary(void);
void capability_print_hints(void);

#endif
