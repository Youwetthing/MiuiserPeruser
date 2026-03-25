
// --- METALHEAD (MIUI subsystem) ---------------------------------------------

// Metalhead comes online
if (strcmp(msg, "METALHEAD=online") == 0) {
    log_message("SYSTEM", "Metalhead online");
    april_broadcast("MATT_DAEMON=online");
    return;
}

// MIUI key/value updates
if (strstr(msg, "MIUI_OPTIMIZATION=")     ||
    strstr(msg, "POWER_OPTIMIZATION=")    ||
    strstr(msg, "AUTOSTART=")             ||
    strstr(msg, "HIDDEN_API_POLICY=")     ||
    strstr(msg, "DUAL_APPS=")             ||
    strstr(msg, "GAME_TURBO=")            ||
    strstr(msg, "BG_RESTRICTION=")) {

    log_message("METALHEAD", msg);
    april_broadcast(msg);
    return;
}

