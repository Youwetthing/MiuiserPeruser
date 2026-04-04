#include "../sensei_core.h"
#include <string.h>

/**
 * Maps shorthand prompts to internal Syndicate commands.
 * "Miui/F" -> FULL_SCAN
 * "Miui/K" -> KILL_TRAITORS
 */
const char* map_human_prompt(const char* prompt) {
    if (strcmp(prompt, "Miui/F") == 0) return "SCAN_ALL";
    if (strcmp(prompt, "Miui/K") == 0) return "REAP_BLOAT";
    if (strcmp(prompt, "Miui/P") == 0) return "PERF_MAX";
    return prompt; // Default to raw command
}

bool execute_human_command(const char* prompt) {
    const char* internal_cmd = map_human_prompt(prompt);
    // Logic to dispatch to Sewer...
    return true;
}
