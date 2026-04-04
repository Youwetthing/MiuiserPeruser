#ifndef SPLINTER_BRAIN_H
#define SPLINTER_BRAIN_H

/* Splinter Brain main hook.
 *
 * fd_bus: the Turtlecom bus socket
 * line:   a single line read from the bus (NUL-terminated)
 *
 * Returns:
 *   0 -> line was handled by Splinter Brain (no further processing)
 *   1 -> line was NOT handled; splinterd may apply legacy logic / Krang
 */
int splinter_brain_handle_line(int fd_bus, const char *line);

#endif /* SPLINTER_BRAIN_H */
