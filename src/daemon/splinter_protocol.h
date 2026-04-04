#ifndef SPLINTER_PROTOCOL_H
#define SPLINTER_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define SPLINTER_MAGIC 0x53504C49
#define MAX_PACKET_SIZE 1024

typedef struct {
    uint32_t command_id;
    char payload[MAX_PACKET_SIZE];
} splinter_packet_t;

// Function Signatures
int splinter_send_packet(splinter_packet_t *pkt);
int splinter_receive_packet(splinter_packet_t *pkt);

// The missing pieces for capabilities_extra.c
bool splinter_protocol_probe(void);
bool splinter_protocol_basic_info(void);

#endif
