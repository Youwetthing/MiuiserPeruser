#include "rish_pipe.h"
#include <stdlib.h>

char *run_shell_cmd(const char *cmd);

char *rish_pipe_command(const char *cmd) {
    return run_shell_cmd(cmd);
}
