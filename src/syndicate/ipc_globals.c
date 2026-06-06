#include "ipc_globals.h"
#include <pthread.h>
#include <stdbool.h>

pthread_t g_thread;
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
