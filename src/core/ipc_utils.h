#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <unistd.h>
#include <stddef.h>
#include <string.h>

static inline ssize_t read_line(int fd, char *buf, size_t max) {
    size_t i = 0;
    char c;
    while (i < max - 1) {
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

static inline void write_line(int fd, const char *msg) {
    size_t len = strlen(msg);
    write(fd, msg, len);
    if (len == 0 || msg[len - 1] != '\n')
        write(fd, "\n", 1);
}

#endif
