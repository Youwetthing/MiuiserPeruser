#ifndef BEBOPD_INTEL_H
#define BEBOPD_INTEL_H

typedef struct {
    int capacity;    /* 0-100% */
    int temp;        /* Battery temp (normalized) */
    int is_charging; /* 1 = AC/USB, 0 = Battery */
} bebopd_packet_t;

#define BEBOPD_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/bebopd.packet"

#endif
