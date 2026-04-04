#include "rish_pipe.h"
/* Core-owned pipe state */

#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>

/* Core-owned pipe state */
pid_t rish_pid = -1;
int rish_stdin = -1;
int rish_stdout = -1;

/* --- rest of your existing implementation goes below this line --- */

