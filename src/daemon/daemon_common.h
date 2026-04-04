#ifndef DAEMON_COMMON_H
#define DAEMON_COMMON_H

/* Unified Sewer Path for Turtlecom Bus */
const char* get_sewer_pipe(void);

/* Ensure environment is prepped for the Bus */
void sewer_prep(void);

/* Every worker calls this to say "I'm here" */
void worker_announce(const char* name, const char* capability);

#endif
