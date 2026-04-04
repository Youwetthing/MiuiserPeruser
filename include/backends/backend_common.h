#ifndef BACKEND_COMMON_H
#define BACKEND_COMMON_H

// Syndicate Backend Ranks
typedef enum {
    BACKEND_NONE = 0,
    BACKEND_RISH,      // Privileged (The Shredder/Shizuku)
    BACKEND_ADB,       // Standard (The Foot Soldier)
    BACKEND_TERMUX     // Local (The Sewer)
} backend_type_t;

// The VTable (The Strategy Guide)
typedef struct {
    const char* name;
    int (*init)(void);
    int (*send_cmd)(const char* cmd);
} backend_vtable_t;

#define TECHNODROME_VER "Mouser-D9"
#endif
