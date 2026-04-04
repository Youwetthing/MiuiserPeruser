#ifndef APRIL_TABLE_H
#define APRIL_TABLE_H

#include <stdint.h>

// This struct is exactly 1024 bytes (1KB) for easy mmap alignment
typedef struct {
    uint32_t magic_v1;        // Constant: 0xDEADBEEF
    uint8_t  daemon_active;   // 1 = Running, 0 = Stalled
    uint8_t  threat_level;    // 0-255 (Calculated by Don/Leo)
    
    // The "Turtle" Status Bits
    struct {
        uint8_t leo_scanning;
        uint8_t raph_hooked;
        uint8_t don_memory_ok;
        uint8_t mikey_stealth;
    } status;

    char current_target[256]; // The package name currently being "perused"
    uint64_t total_hits;      // Counter for total debloats in session
    
    uint8_t padding[750];     // Reserved for future "Sewer" tech
} AprilTable;

#endif