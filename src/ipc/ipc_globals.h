#pragma once

#include <stddef.h>

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

#define PORTBRIDGE_SOCKET  BASE "/pipes/portbridge.sock"
#define METALHEAD_SOCKET   BASE "/pipes/metalheadd.sock"

#define PIDS_DIR           BASE "/pipes/pids"
#define KRANG_PID          PIDS_DIR "/krangd.pid"
#define TURTLECOM_PID      PIDS_DIR "/turtlecomd.pid"
#define SHREDDERD_PID      PIDS_DIR "/shredderd.pid"
#define GRANITORD_PID      PIDS_DIR "/granitord.pid"
#define BURNED_PID         PIDS_DIR "/burned.pid"
#define RAHZERD_PID        PIDS_DIR "/rahzerd.pid"
#define RATKINGD_PID       PIDS_DIR "/ratkingd.pid"
#define BEBOPD_PID         PIDS_DIR "/bebopd.pid"
#define FUGITOIDD_PID      PIDS_DIR "/fugitoidd.pid"
#define LEATHERHEADD_PID   PIDS_DIR "/leatherheadd.pid"
#define METALHEADD_PID     PIDS_DIR "/metalheadd.pid"
#define ROCKSTEADYD_PID    PIDS_DIR "/rocksteadyd.pid"
#define TIGERCLAWD_PID     PIDS_DIR "/tigerclawd.pid"

#define MP_TCP_PORT 6789

#define IPC_CHANNEL_LEATHERHEAD "leatherheadd"
