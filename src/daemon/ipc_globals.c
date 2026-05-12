#include "ipc_globals.h"

volatile bool g_running = false;
pthread_t g_thread;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
