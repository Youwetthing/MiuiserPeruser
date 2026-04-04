#include <leo_detection.h>
#include <sensei_types.h>
#include "rish_pipe.h"
#include <string.h>
#include <stdlib.h>

SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *results) {
    char *out = rish_pipe_command("cmd appops get com.termux RUN_IN_BACKGROUND");
    if (out) {
        if (strstr(out, "deny")) {
            SENSEI_DETECTION det = {.priority = SENSEI_EVENT_PRIORITY_HIGH, .confidence = 95};
            strncpy(det.detection_type, "RESTRICTED_BACKGROUND", 63);
            snprintf(det.description, 511, "MIUI background restrictions detected for Termux.");
            leo_detection_list_append(results, &det);
        }
        free(out);
    }
    return SENSEI_STATUS_OK;
}
