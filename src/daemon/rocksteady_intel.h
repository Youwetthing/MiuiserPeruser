#ifndef ROCKSTEADY_INTEL_H
#define ROCKSTEADY_INTEL_H

typedef struct {
    float load_avg;      /* The 1-min load */
    int freq_mhz;        /* Current clock speed */
    int pressure_score;  /* 0-100 normalized stress */
} rocksteady_packet_t;

/* Path to the Scout's data packet */
#define ROCKSTEADY_PACKET "/data/data/com.termux/files/home/.syndicate_sewer/rocksteady.packet"

#endif
