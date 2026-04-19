#ifndef TURTLE_CORE_PATHS_H
#define TURTLE_CORE_PATHS_H

/* =========================
   CORE SYSTEM PATHS
   ========================= */

#define TURTLE_HOME   "/data/data/com.termux/files/home/MiuiserPeruser"

#define STATE_DIR     TURTLE_HOME "/state"
#define CORE_DIR      TURTLE_HOME "/src/core"
#define TMP_DIR       TURTLE_HOME "/tmp"

/* Event system */
#define COURT_EVENTS      STATE_DIR "/court.events"
#define COURT_CLOSED      STATE_DIR "/court.events.closed"

/* Core lock */
#define TURTLE_LOCK       CORE_DIR  "/turtlepower.lock"

/* IPC */
#define KRANG_SOCKET      TMP_DIR   "/krang.sock"
#define TURTLECOM_SOCKET  TMP_DIR   "/turtlecom.sock"

/* PID helpers macro (used by all daemons) */
#define PID_FILE(name)   TMP_DIR "/" name ".pid"

#endif
