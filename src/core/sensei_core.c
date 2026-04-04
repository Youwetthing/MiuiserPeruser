#include "sensei_core.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

static void* april_broadcast_map = NULL;

int sensei_core_init(void) {
    /* Standard file-backed mmap for Termux compatibility */
    const char* april_path = "/data/data/com.termux/files/home/tmp/miuiser_april.bin";
    
    /* Ensure tmp directory exists */
    mkdir("/data/data/com.termux/files/home/tmp", 0700);

    int fd = open(april_path, O_RDWR | O_CREAT, 0666);
    if (fd >= 0) {
        /* Set table size to 4KB */
        ftruncate(fd, 4096);
        april_broadcast_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        
        if (april_broadcast_map != MAP_FAILED) {
            return 0; /* Heartbeat initialized */
        }
    }
    perror("Sensei Core: April Table mapping failed");
    return -1;
}
