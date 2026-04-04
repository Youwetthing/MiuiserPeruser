#pragma once

#include <pthread.h>
#include <stdbool.h>

extern volatile bool g_running;
extern pthread_t g_thread;
extern pthread_mutex_t g_mutex;

#define IPC_CHANNEL_LEATHERHEAD "leatherheadd"
#define DASHBOARD_SOCKET "/data/data/com.termux/files/home/MiuiserPeruser/tmp/dashboard.sock"
#define SEWER_SOCKET     "/data/data/com.termux/files/home/MiuiserPeruser/tmp/sewer.sock"
#define LEATHERHEAD_SOCKET "/data/data/com.termux/files/home/MiuiserPeruser/tmp/leatherhead.sock"
