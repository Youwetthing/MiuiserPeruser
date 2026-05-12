#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "backend_portbridge.h"

#define PORTBRIDGE_SOCK "/data/local/tmp/portbridge.sock"

char *backend_portbridge_probe(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return strdup("portbridge:error");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, PORTBRIDGE_SOCK, sizeof(addr.sun_path) - 1);

    int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    close(fd);

    if (r == 0)
        return strdup("portbridge:ok");

    if (access(PORTBRIDGE_SOCK, F_OK) != 0)
        return strdup("portbridge:missing");

    if (r < 0)
        return strdup("portbridge:refused");

    return strdup("portbridge:error");
}
