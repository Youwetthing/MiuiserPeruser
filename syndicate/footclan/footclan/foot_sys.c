// foot_sys.c — Footrunner sysport shim
// Centralizes all system reads so jobs don't care if it's direct or via sysport.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// sysport client API
char *sysport_read_sys(const char *path);
char *sysport_read_proc(const char *path);
char *sysport_getprop(const char *key);
char *sysport_dumpsys(const char *service);

// ------------------------------------------------------------
// Helpers: all Foot jobs should use these
// ------------------------------------------------------------

char *foot_read_sys(const char *path) {
    return sysport_read_sys(path);
}

char *foot_read_proc(const char *path) {
    return sysport_read_proc(path);
}

char *foot_getprop(const char *key) {
    return sysport_getprop(key);
}

char *foot_dumpsys(const char *service) {
    return sysport_dumpsys(service);
}
