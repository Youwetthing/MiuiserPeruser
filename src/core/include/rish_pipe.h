#ifndef RISH_PIPE_H
#define RISH_PIPE_H

#include <sys/types.h>

extern pid_t rish_pid;
extern int rish_stdin;
extern int rish_stdout;

int rish_pipe_start(void);
char *rish_pipe_command(const char *cmd);
void rish_pipe_stop(void);

#endif
