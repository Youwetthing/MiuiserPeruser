#pragma once

#include <pthread.h>
#include <stdbool.h>

extern volatile bool g_running;
extern pthread_t g_thread;
extern pthread_mutex_t g_mutex;

/* ── Project root ─────────────────────────────────────────────────────── */
#ifndef BASE
#define BASE         "/data/data/com.termux/files/home/MiuiserPeruser"
#endif
#define MP_PIPES_DIR BASE "/pipes"
#define MP_PIDS_DIR  BASE "/pipes/pids"

/* ── Unix socket paths ────────────────────────────────────────────────── */
#define KRANG_SOCKET       MP_PIPES_DIR "/krang.sock"
#define TURTLE_SOCKET      MP_PIPES_DIR "/turtlecom.sock"
#define DASHBOARD_SOCKET   MP_PIPES_DIR "/dashboard.sock"
#define SEWER_SOCKET       MP_PIPES_DIR "/sewer.sock"
#define LEATHERHEAD_SOCKET MP_PIPES_DIR "/leatherhead.sock"
#define SPLINTER_SOCKET    MP_PIPES_DIR "/splinterd.sock"
#define RAHZER_SOCKET      MP_PIPES_DIR "/rahzerd.sock"
#define BEBOP_SOCKET       MP_PIPES_DIR "/bebopd.sock"
#define GRANITOR_SOCKET    MP_PIPES_DIR "/granitord.sock"
#define SHREDDER_SOCKET    MP_PIPES_DIR "/shredderd.sock"
#define FUGITOID_SOCKET    MP_PIPES_DIR "/fugitoidd.sock"
#define BURNED_SOCKET      MP_PIPES_DIR "/burned.sock"
#define ROCKSTEADY_SOCKET  MP_PIPES_DIR "/rocksteadyd.sock"
#define RATKING_SOCKET     MP_PIPES_DIR "/ratkingd.sock"
#define TIGERCLAW_SOCKET   MP_PIPES_DIR "/tigerclawd.sock"
#define METALHEAD_SOCKET   MP_PIPES_DIR "/metalheadd.sock"

/* ── PID file paths ───────────────────────────────────────────────────── */
#define KRANG_PID          MP_PIDS_DIR "/krangd.pid"
#define SPLINTER_PID       MP_PIDS_DIR "/splinterd.pid"
#define RAHZERD_PID        MP_PIDS_DIR "/rahzerd.pid"

/* ── TCP fallback ─────────────────────────────────────────────────────── */
#define MP_TCP_PORT 6789

#define IPC_CHANNEL_LEATHERHEAD "leatherheadd"
