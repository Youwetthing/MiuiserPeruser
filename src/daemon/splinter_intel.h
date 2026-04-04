#ifndef SPLINTER_INTEL_H
#define SPLINTER_INTEL_H

typedef struct {
    int harmony_score; // 0 (Peace) to 100 (Chaos)
    int sentinel_count;
} splinter_packet_t;

#define SPLINTER_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/splinterd.packet"

#endif
