#include "leo_detection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

SENSEI_STATUS leo_detection_list_append(SENSEI_DETECTION_LIST *list, SENSEI_DETECTION *det) {
    if (!list || !det) return SENSEI_STATUS_ERROR;
    SENSEI_DETECTION *new_det = malloc(sizeof(SENSEI_DETECTION));
    if (!new_det) return SENSEI_STATUS_NO_MEMORY;
    memcpy(new_det, det, sizeof(SENSEI_DETECTION));
    new_det->next = NULL;
    if (!list->head) list->head = new_det;
    else {
        SENSEI_DETECTION *curr = list->head;
        while (curr->next) curr = curr->next;
        curr->next = new_det;
    }
    list->count++;
    return SENSEI_STATUS_OK;
}

void leo_detection_list_free(SENSEI_DETECTION_LIST *list) {
    if (!list) return;
    SENSEI_DETECTION *curr = list->head;
    while (curr) {
        SENSEI_DETECTION *next = curr->next;
        free(curr);
        curr = next;
    }
    list->head = NULL;
    list->count = 0;
}

/* REAL Shell Pipe Execution */
SENSEI_STATUS rish_pipe_command(const char* cmd, char* result, size_t size) {
    if (!cmd || !result || size == 0) return SENSEI_STATUS_ERROR;
    memset(result, 0, size);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return SENSEI_STATUS_ERROR;
    
    size_t bytes_read = fread(result, 1, size - 1, fp);
    pclose(fp);
    
    return (bytes_read > 0) ? SENSEI_STATUS_OK : SENSEI_STATUS_NOT_FOUND;
}

/* Unrooted Stubs */
SENSEI_STATUS raph_memory_scan(int pid, SENSEI_DETECTION_LIST *res) { (void)pid; (void)res; return SENSEI_STATUS_OK; }
SENSEI_STATUS raph_network_scan(SENSEI_DETECTION_LIST *res) { (void)res; return SENSEI_STATUS_OK; }
SENSEI_STATUS mikey_miui(SENSEI_DETECTION_LIST *res) { (void)res; return SENSEI_STATUS_OK; }
SENSEI_STATUS don_memorypressure_check(SENSEI_DETECTION_LIST *res) { (void)res; return SENSEI_STATUS_OK; }
SENSEI_STATUS april_process_list_append(SENSEI_PROCESS_LIST *list, SENSEI_PROCESS_INFO *info) { return SENSEI_STATUS_OK; }
void april_process_list_free(SENSEI_PROCESS_LIST *list) { (void)list; }
void april_memory_region_list_free(SENSEI_MEMORY_REGION *head) { (void)head; }
