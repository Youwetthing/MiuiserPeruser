#pragma once

#include <pthread.h>
#include <stdbool.h>

extern volatile bool g_running;
extern pthread_t g_thread;
extern pthread_mutex_t g_mutex;

#define BASE "/data/data/com.termux/files/home/MiuiserPeruser"

#define KRANG_SOCKET       BASE "/pipes/krang.sock"
#define TURTLE_SOCKET      BASE "/pipes/turtlecom.sock"
#define DASHBOARD_SOCKET   BASE "/pipes/dashboard.sock"
#define SEWER_SOCKET       BASE "/pipes/sewer.sock"
#define LEATHERHEAD_SOCKET BASE "/pipes/leatherhead.sock"
#define SPLINTER_SOCKET    BASE "/pipes/splinterd.sock"
#define RAHZER_SOCKET      BASE "/pipes/rahzerd.sock"
#define BEBOP_SOCKET       BASE "/pipes/bebopd.sock"
#define GRANITOR_SOCKET    BASE "/pipes/granitord.sock"
#define SHREDDER_SOCKET    BASE "/pipes/shredderd.sock"
#define FUGITOID_SOCKET    BASE "/pipes/fugitoidd.sock"
#define BURNED_SOCKET      BASE "/pipes/burned.sock"
#define ROCKSTEADY_SOCKET  BASE "/pipes/rocksteadyd.sock"
#define RATKING_SOCKET     BASE "/pipes/ratkingd.sock"
#define TIGERCLAW_SOCKET   BASE "/pipes/tigerclawd.sock"

#define MP_TCP_PORT 6789

#define IPC_CHANNEL_LEATHERHEAD "leatherheadd"
