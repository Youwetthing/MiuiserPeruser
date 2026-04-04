#include "ipc_globals.h"

volatile bool g_running = false;
pthread_t g_thread;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global relay for krang commands
int krang_send_command(const char *cmd) {
    if (!cmd) return -1;
    // Implementation is linked from krang logic or stubbed here
    return 0;
}
int g_dashboard_listen_fd = -1;
