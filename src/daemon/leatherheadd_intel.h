#ifndef LEATHERHEADD_INTEL_H
#define LEATHERHEADD_INTEL_H

typedef struct {
    int core_temp;       /* CPU Temperature in Celsius */
    int battery_temp;    /* Battery Temperature in Celsius */
    int fever_score;     /* 0-100 normalized thermal stress */
} leatherheadd_packet_t;

#define LEATHERHEADD_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/leatherheadd.packet"

#endif
