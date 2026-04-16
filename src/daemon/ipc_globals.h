#ifndef IPC_GLOBALS_H
#define IPC_GLOBALS_H

#include <pthread.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool g_running;
extern pthread_t g_thread;
extern pthread_mutex_t g_mutex;

/* Absolute Termux Paths - The "Sewer" */
#define DASHBOARD_SOCKET "/data/data/com.termux/files/home/tmp/dashboard.sock"
#define SEWER_SOCKET     "/data/data/com.termux/files/home/tmp/sewer.sock"
#define KRANG_PATH "/data/data/com.termux/files/home/tmp/krang.sock"

#ifdef __cplusplus
}
#endif

#endif
