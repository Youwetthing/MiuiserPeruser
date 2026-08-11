/*
 * footclan.c — Event logging daemon for MiuiserPeruser
 *
 * Connects to splinterd on pipes/splinter.sock
 * Receives APRIL events: APRIL|source|type|payload\n
 * Stores them in logs/syndicate_footclan.db
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdarg.h>

#include "core_paths.h"

#define MP_PIPES_DIR      TURTLE_HOME "/pipes"
#define SPLINTER_SOCKET   MP_PIPES_DIR "/splinterd.sock"
#define MP_PIDS_DIR       TURTLE_HOME "/pipes/pids"
#define DB_PATH           TURTLE_HOME "/logs/syndicate_footclan.db"
#define LOG_PATH          TURTLE_HOME "/logs/footclan.log"

#define RECV_BUF_SIZE     2048
#define APRIL_PREFIX      "APRIL"

static volatile bool g_running = true;
static sqlite3 *g_db = NULL;
static FILE *g_log_fp = NULL;

static void footclan_log(const char *level, const char *fmt, ...) {
    char tsbuf[32];
    time_t t = time(NULL);
    strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%dT%H:%M:%S", localtime(&t));
    
    va_list ap;
    va_start(ap, fmt);
    
    fprintf(stderr, "[%s][FOOTCLAN/%s] ", tsbuf, level);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    
    if (g_log_fp) {
        fprintf(g_log_fp, "[%s][FOOTCLAN/%s] ", tsbuf, level);
        vfprintf(g_log_fp, fmt, ap);
        fprintf(g_log_fp, "\n");
        fflush(g_log_fp);
    }
    
    va_end(ap);
}

static int db_init(void) {
    int rc = sqlite3_open(DB_PATH, &g_db);
    if (rc != SQLITE_OK) {
        footclan_log("ERROR", "Failed to open DB: %s", sqlite3_errmsg(g_db));
        return -1;
    }
    
    const char *schema = 
        "CREATE TABLE IF NOT EXISTS events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp TEXT DEFAULT CURRENT_TIMESTAMP,"
        "  source TEXT NOT NULL,"
        "  type TEXT NOT NULL,"
        "  payload TEXT"
        ");";
    
    char *errmsg = NULL;
    rc = sqlite3_exec(g_db, schema, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        footclan_log("ERROR", "Failed to create schema: %s", errmsg);
        sqlite3_free(errmsg);
        return -1;
    }
    
    footclan_log("INFO", "Database initialized at %s", DB_PATH);
    return 0;
}

static int db_insert_event(const char *source, const char *type, const char *payload) {
    const char *sql = 
        "INSERT INTO events (source, type, payload) VALUES (?, ?, ?);";
    
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        footclan_log("ERROR", "Failed to prepare statement: %s", sqlite3_errmsg(g_db));
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, source, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, payload, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        footclan_log("ERROR", "Failed to insert event: %s", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

static int parse_april_event(const char *line, char *source, char *type, char *payload) {
    if (strncmp(line, APRIL_PREFIX, strlen(APRIL_PREFIX)) != 0) {
        return -1;
    }
    
    const char *p = line + strlen(APRIL_PREFIX);
    if (*p != '|') return -1;
    p++;
    
    const char *end = strchr(p, '|');
    if (!end || (end - p) >= 64) return -1;
    strncpy(source, p, end - p);
    source[end - p] = '\0';
    p = end + 1;
    
    end = strchr(p, '|');
    if (!end || (end - p) >= 64) return -1;
    strncpy(type, p, end - p);
    type[end - p] = '\0';
    p = end + 1;
    
    int len = strlen(p);
    if (len > 0 && p[len - 1] == '\n') len--;
    if (len >= 1024) len = 1023;
    strncpy(payload, p, len);
    payload[len] = '\0';
    
    return 0;
}

static void sig_handler(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        g_running = false;
    }
}

static int connect_to_splinterd(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        footclan_log("ERROR", "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SPLINTER_SOCKET, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        footclan_log("ERROR", "Failed to connect to splinterd at %s: %s", 
                     SPLINTER_SOCKET, strerror(errno));
        close(sock);
        return -1;
    }
    
    footclan_log("INFO", "Connected to splinterd at %s", SPLINTER_SOCKET);
    return sock;
}

static void event_loop(int sock) {
    char buf[RECV_BUF_SIZE];
    ssize_t n;
    
    while (g_running) {
        n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            footclan_log("ERROR", "recv failed: %s", strerror(errno));
            break;
        }
        if (n == 0) {
            footclan_log("INFO", "splinterd closed connection");
            break;
        }
        
        buf[n] = '\0';
        
        char *line = buf;
        char *newline;
        while ((newline = strchr(line, '\n')) != NULL) {
            *newline = '\0';
            
            if (strlen(line) > 0) {
                char source[64], type[64], payload[1024];
                if (parse_april_event(line, source, type, payload) == 0) {
                    if (db_insert_event(source, type, payload) == 0) {
                        footclan_log("DEBUG", "Logged: %s|%s", source, type);
                    }
                }
            }
            
            line = newline + 1;
        }
        
        if (strlen(line) > 0) {
            memcpy(buf, line, strlen(line) + 1);
        }
    }
}

static void write_pidfile(void) {
    FILE *f = fopen(MP_PIDS_DIR "/footclan.pid", "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static void cleanup(void) {
    if (g_db) sqlite3_close(g_db);
    if (g_log_fp) fclose(g_log_fp);
    unlink(MP_PIDS_DIR "/footclan.pid");
    footclan_log("INFO", "footclan shutdown");
}

int main(void) {
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    
    g_log_fp = fopen(LOG_PATH, "a");
    
    footclan_log("INFO", "footclan starting");
    
    if (db_init() < 0) {
        cleanup();
        return 1;
    }
    
    write_pidfile();
    
    int sock = -1;
    for (int i = 0; i < 5; i++) {
        sock = connect_to_splinterd();
        if (sock >= 0) break;
        sleep(1);
    }
    
    if (sock < 0) {
        footclan_log("ERROR", "Failed to connect to splinterd after retries");
        cleanup();
        return 1;
    }
    
    event_loop(sock);
    
    close(sock);
    cleanup();
    return 0;
}
