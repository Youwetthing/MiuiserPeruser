#include "april_table.h"
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

uint32_t april_read(int offset, uint32_t default_val) {
    int fd = open(APRIL_BIN, O_RDONLY);
    if (fd < 0) return default_val;
    if (lseek(fd, offset, SEEK_SET) < 0) { close(fd); return default_val; }
    uint32_t raw = 0;
    ssize_t n = read(fd, &raw, sizeof(raw));
    close(fd);
    if (n != sizeof(raw)) return default_val;
    return ntohl(raw);
}

unsigned int april_poll_sleep(unsigned int base_seconds) {
    uint32_t throttle = april_read(APRIL_POLL_THROTTLE, POLL_NORMAL);
    switch (throttle) {
        case POLL_THROTTLED: return base_seconds * 3;
        case POLL_MINIMAL:   return base_seconds * 10;
        default:             return base_seconds;
    }
}
