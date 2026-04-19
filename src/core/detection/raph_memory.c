#include <stdio.h>

void raph_memory_scan(uint32_t pid, SENSEI_DETECTION_LIST *results) {
    SENSEI_DETECTION det = {0};

    // Ask platform to populate detections (acts as backend probe)
    if (april_enum_memory_regions(pid, results) != SENSEI_STATUS_OK) {
        return;
    }

    // Lightweight heuristic fallback (placeholder until full memory API exists)
    det.detection_class = SENSEI_DETECTION_CLASS_MEMORY;
    det.mitre_id = SENSEI_MITRE_T1055;
    snprintf(det.description, sizeof(det.description),
             "Memory scan executed via April backend (pid=%u)", pid);

    april_detection_list_append(results, &det);
}
