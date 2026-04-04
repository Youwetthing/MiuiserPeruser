#ifndef RATKING_INTEL_H
#define RATKING_INTEL_H

typedef struct {
    int pid;
    char name[64];
    float cpu_usage;
} ratking_packet_t;

#define RATKING_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/ratkingd.packet"

#endif
