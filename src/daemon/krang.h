#ifndef KRANG_H
#define KRANG_H

/* Krang – the sewer IPC client for worker daemons. */

/* Connect to the master's sewer socket. Returns socket fd, or -1 on error. */
int krang_connect(void);

/* Send a command and receive the full response (must be freed). */
char* krang_send_command(const char* cmd);

/* Close the sewer connection. */
void krang_disconnect(void);

#endif
