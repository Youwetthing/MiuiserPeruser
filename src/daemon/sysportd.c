#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9999);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) return 1;
    listen(srv, 5);

    while (1) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;

        uint32_t bin_code = 0;
        char response[64] = "ERROR: UNKNOWN_OPCODE"; // Default response
        
        // Read the 4-byte opcode
        if (read(client, &bin_code, sizeof(bin_code)) > 0) {
            if (bin_code == 1) { // 0x01
                FILE *f = fopen("/proc/loadavg", "r");
                if (f) {
                    memset(response, 0, 64);
                    fgets(response, 63, f);
                    fclose(f);
                } else { strcpy(response, "ERROR: CANNOT_READ_PROC"); }
            }
        }
        
        // Always send exactly 64 bytes to prevent client hang
        write(client, response, 64);
        close(client);
    }
    return 0;
}
