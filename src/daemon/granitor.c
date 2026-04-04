#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define PIPE_PATH "/data/local/tmp/.hitlist"

int main() {
    char target[256];
    int fd;

    while(1) {
        // Open pipe and wait for the Battle Shell to talk
        fd = open(PIPE_PATH, O_RDONLY);
        if (fd < 0) { sleep(1); continue; }

        ssize_t num_read = read(fd, target, sizeof(target) - 1);
        if (num_read > 0) {
            target[num_read] = '\0';
            target[strcspn(target, "\n\r")] = 0; // Clean the string

            if(strlen(target) > 3) {
                char puncture[512];
                // THE KARAI PUNCTURE
                sprintf(puncture, "am force-stop %s && pm uninstall --user 0 %s > /dev/null 2>&1", target, target);
                system(puncture);
            }
        }
        close(fd);
    }
    return 0;
}
