#ifndef RISH_PIPE_H
#define RISH_PIPE_H

#include <sys/types.h>

int rish_pipe_start(void);
char *rish_pipe_command(const char *cmd);
void rish_pipe_stop(void);

#endif
