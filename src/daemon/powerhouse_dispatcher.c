#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "action_matrix.h"

int dispatch_to_sewer(action_id_t action_id) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 'ping' > /data/data/com.termux/files/home/.syndicate_sewer/q.ready");
    return system(cmd);
}
