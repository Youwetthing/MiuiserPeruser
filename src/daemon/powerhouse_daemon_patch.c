#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void execute_policy(const char *action_cmd) {
    char cmd_buffer[256];
    // This drops the command into the Sewer Pipe (q.ready)
    snprintf(cmd_buffer, sizeof(cmd_buffer), 
             "echo '%s' > /data/local/tmp/syndicate/q.ready", action_cmd);
    
    printf("[POWERHOUSE] Sending Action to Sewer: %s\n", action_cmd);
    system(cmd_buffer);
}
