#ifndef KRANG_H
#define KRANG_H

// Establishes connection to Krang.
// Returns 0 on success, non‑zero on failure.
int krang_connect(void);

// Sends a command to Krang and returns a malloc'd response string.
// Caller must free() the returned buffer.
char *krang_send_command(const char *cmd);

#endif // KRANG_H

