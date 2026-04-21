#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backend/backend_exec.h"

#define ALPHA 0.2f
#define THRESHOLD 1.2f
#define STREAK_LIMIT 4

#define W_WIFI   0.6f
#define W_BT     0.5f
#define W_NFC    0.2f
#define W_CONN   0.7f
#define W_NET    0.8f
#define W_RADIO  1.0f

static float base_wifi = 0.0f;
static float base_bt = 0.0f;
static float base_nfc = 0.0f;
static float base_conn = 0.0f;
static float base_net = 0.0f;
static float base_radio = 0.0f;

static int anomaly_streak = 0;

static float ema(float prev, float x) {
    return ALPHA * x + (1.0f - ALPHA) * prev;
}

static float detect_change(const char *old, const char *new) {
    if (!old || strlen(old) == 0) return 1.0f;
    return (strcmp(old, new) != 0) ? 1.0f : 0.0f;
}

static void emit_case_packet(float score, int streak) {
    printf("\n[TURTLEPOWER PACKET]\n");
    printf("{\n");
    printf("  \"source\": \"rahzerd\",\n");
    printf("  \"score\": %.2f,\n", score);
    printf("  \"streak\": %d,\n", streak);
    printf("  \"status\": \"case_candidate\",\n");
    printf("  \"priority\": \"unclassified\"\n");
    printf("}\n\n");
}

void scan_environment() {
    printf("[RAHZERD] backend-gated adaptive scan\n");

    char old_state[4096] = "";

    FILE *f = fopen("src/syndicate/rahzerd/state/rahzerd.json", "r");
    if (f) {
        fread(old_state, 1, sizeof(old_state) - 1, f);
        fclose(f);
    }

    char *wifi  = backend_exec("dumpsys wifi | head -5");
    char *bt    = backend_exec("dumpsys bluetooth_manager | head -5");
    char *nfc   = backend_exec("dumpsys nfc | head -5");
    char *conn  = backend_exec("dumpsys connectivity | head -5");
    char *net   = backend_exec("dumpsys netstats | head -5");
    char *radio = backend_exec("dumpsys telephony.registry | head -5");

    float x_wifi  = detect_change(old_state, wifi);
    float x_bt    = detect_change(old_state, bt);
    float x_nfc   = detect_change(old_state, nfc);
    float x_conn  = detect_change(old_state, conn);
    float x_net   = detect_change(old_state, net);
    float x_radio = detect_change(old_state, radio);

    base_wifi  = ema(base_wifi, x_wifi);
    base_bt    = ema(base_bt, x_bt);
    base_nfc   = ema(base_nfc, x_nfc);
    base_conn  = ema(base_conn, x_conn);
    base_net   = ema(base_net, x_net);
    base_radio = ema(base_radio, x_radio);

    float score =
        base_wifi  * W_WIFI +
        base_bt    * W_BT +
        base_nfc   * W_NFC +
        base_conn  * W_CONN +
        base_net   * W_NET +
        base_radio * W_RADIO;

    printf("[RAHZERD][SCORE] %.2f\n", score);
    printf("[RAHZERD][STREAK] %d\n", anomaly_streak);

    if (score > THRESHOLD)
        anomaly_streak++;
    else if (anomaly_streak > 0)
        anomaly_streak--;

    if (score > THRESHOLD && anomaly_streak >= STREAK_LIMIT) {
        emit_case_packet(score, anomaly_streak);
    } else {
        printf("[RAHZERD] stable or transient state\n");
    }

    free(wifi);
    free(bt);
    free(nfc);
    free(conn);
    free(net);
    free(radio);
}
