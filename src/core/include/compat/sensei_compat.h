#include <stdio.h>
#pragma once

#include <stdio.h>
#include <stdint.h>
#include "sensei_types.h"

/* ---- PROCESS ---- */
#ifndef SENSEI_PROCESS_INFO
typedef struct {
    uint32_t pid;
    uint32_t ppid;
    char name[128];
    char path[256];
} SENSEI_PROCESS_INFO;
#endif

/* ---- MEMORY ---- */
#ifndef SENSEI_MEMORY_REGION
typedef struct SENSEI_MEMORY_REGION {
    void *base_address;
    size_t size;
    int prot;
    struct SENSEI_MEMORY_REGION *next;
} SENSEI_MEMORY_REGION;
#endif

/* ---- ENUM FALLBACKS ---- */
#ifndef SENSEI_DETECTION_CLASS_MEMORY
#define SENSEI_DETECTION_CLASS_MEMORY 2
#endif

#ifndef SENSEI_MITRE_T1036
#define SENSEI_MITRE_T1036 1036
#endif

#ifndef SENSEI_MITRE_T1055_001
#define SENSEI_MITRE_T1055_001 SENSEI_MITRE_T1055
#endif
