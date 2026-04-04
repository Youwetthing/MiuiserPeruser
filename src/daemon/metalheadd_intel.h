#ifndef METALHEADD_INTEL_H
#define METALHEADD_INTEL_H

typedef struct {
    int iowait_percent;  /* 0-100% CPU waiting on disk */
    int free_mb;         /* Available storage in Megabytes */
} metalheadd_packet_t;

#define METALHEADD_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/metalheadd.packet"

#endif
