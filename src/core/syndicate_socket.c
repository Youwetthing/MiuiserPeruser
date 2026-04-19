#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/syndicate.sock"

void syndicate_broadcast(const char *msg) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) {
        send(sock, msg, strlen(msg), 0);
    }
    close(sock);
}
