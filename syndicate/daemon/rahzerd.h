/*
 * rahzerd.h  v2  —  Full-Spectrum Connectivity Audit Daemon
 * MiuiserPeruser / MIUI / HyperOS
 *
 * 15 communication layers:
 *   1  WiFi               9  UWB ranging
 *   2  Mobile/Radio       10 FM Radio
 *   3  DNS health         11 USB connection
 *   4  Telephone          12 Infrared (IR blaster)
 *   5  SMS                13 Bluetooth + scanning
 *   6  MMS + APN          14 NFC
 *   7  Network ports      15 Xiaomi interference layer
 *   8  Ethernet
 */

#ifndef RAHZERD_H
#define RAHZERD_H

#include <stdint.h>
#include <time.h>

typedef enum {
    RZ_BACKEND_RISH    = 0,
    RZ_BACKEND_DUMPSYS = 1,
    RZ_BACKEND_SYSFS   = 2,
    RZ_BACKEND_NONE    = 3
} rz_backend_t;

static const char *const RZ_BACKEND_NAMES[] = {
    "rish", "dumpsys", "sysfs", "none"
};

typedef enum {
    RZ_CONF_MEASURED = 0,
    RZ_CONF_INFERRED = 1,
    RZ_CONF_FALLBACK = 2,
    RZ_CONF_ABSENT   = 3
} rz_confidence_t;

typedef struct {
    int  connected;
    char ssid[64];
    char bssid[24];
    int  rssi_dbm;
    int  link_speed_mbps;
    int  frequency_mhz;
    char ip4[24];
    char ip6[64];
    long tx_bytes;
    long rx_bytes;
    int  tx_pkts;
    int  rx_pkts;
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_wifi_t;

typedef struct {
    int  sim_present;
    int  data_active;
    int  roaming;
    int  signal_strength;
    int  signal_dbm;
    char operator_name[64];
    char rat_type[32];
    char data_activity[32];
    int  cell_id;
    int  dual_sim;
    char sim2_operator[64];
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_mobile_t;

typedef struct {
    int  resolves;
    int  latency_ms;
    char nameserver_primary[64];
    char nameserver_secondary[64];
    char test_host[128];
    int  fallback_used;
    int  private_dns_active;
    char private_dns_host[128];
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_dns_t;

typedef struct {
    int  call_active;
    int  call_ringing;
    int  call_holding;
    char call_state[32];
    char call_direction[16];
    char remote_number[64];
    int  emergency_only;
    char network_state[32];
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_telephone_t;

typedef struct {
    int  service_available;
    int  sms_capable;
    int  sending_blocked;
    char service_state[32];
    int  queued_count;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_sms_t;

typedef struct {
    int  mms_capable;
    char apn_name[64];
    char mmsc_url[256];
    char mms_proxy[64];
    int  mms_port;
    int  apn_active;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_mms_t;

#define RZ_MAX_OPEN_PORTS    64
#define RZ_MAX_OVERFLOW_FLAG -1

typedef struct {
    int  smtp_25;
    int  smtp_465;
    int  smtp_587;
    int  imap_143;
    int  imap_993;
    int  pop3_110;
    int  pop3_995;
    int  listen_ports[RZ_MAX_OPEN_PORTS];
    int  listen_count;
    int  established_tcp4;
    int  established_tcp6;
    int  suspicious_listeners;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_ports_t;

typedef struct {
    int  present;
    int  connected;
    char ip4[24];
    char mac[24];
    long tx_bytes;
    long rx_bytes;
    int  speed_mbps;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_ethernet_t;

typedef struct {
    int  supported;
    int  enabled;
    int  ranging_active;
    int  session_count;
    char state[32];
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_uwb_t;

typedef struct {
    int   supported;
    int   enabled;
    float frequency_mhz;
    int   stereo;
    int   signal_strength;
    char  state[32];
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_fm_t;

typedef struct {
    int  connected;
    char state[32];
    char mode[64];
    int  adb_active;
    int  tethering_active;
    char usb_speed[32];
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_usb_t;

typedef struct {
    int  supported;
    int  transmitting;
    char device_path[128];
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_infrared_t;

typedef struct {
    int  enabled;
    int  scanning;
    int  advertising;
    int  paired_count;
    int  connected_count;
    char local_name[64];
    char local_mac[24];
    char active_profile[64];
    int  le_supported;
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_bluetooth_t;

typedef struct {
    int  supported;
    int  enabled;
    int  tag_present;
    int  beam_enabled;
    int  hce_active;
    char state[32];
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_nfc_t;

typedef struct {
    int  jx_policy_active;
    int  aml_conn_active;
    int  miui_wifi_active;
    int  powerkeeper_throttling;
    int  turbosched_active;
    int  perfshielder_active;
    int  miuibooster_active;
    int  whetstone_power_active;
    int  smartpower_active;
    char active_interface[32];
    char default_gateway[24];
    int  aosp_reports_connected;
    int  xiaomi_reports_connected;
    int  kernel_reports_connected;
    int  divergence_detected;
    char divergence_reason[128];
    time_t last_change;
    rz_backend_t    backend;
    rz_confidence_t confidence;
} rz_xiaomi_t;

typedef struct {
    time_t         timestamp;
    int            poll_cycle;
    int            poll_duration_ms;
    rz_wifi_t      wifi;
    rz_mobile_t    mobile;
    rz_dns_t       dns;
    rz_telephone_t telephone;
    rz_sms_t       sms;
    rz_mms_t       mms;
    rz_ports_t     ports;
    rz_ethernet_t  ethernet;
    rz_uwb_t       uwb;
    rz_fm_t        fm;
    rz_usb_t       usb;
    rz_infrared_t  infrared;
    rz_bluetooth_t bluetooth;
    rz_nfc_t       nfc;
    rz_xiaomi_t    xiaomi;
} rz_snapshot_t;

typedef struct {
    rz_snapshot_t prev;
    rz_snapshot_t curr;
    int           first_poll;
    time_t        wifi_connected_since;
    time_t        mobile_data_since;
    time_t        dns_failing_since;
    int           wifi_flap_count;
    int           dns_spike_count;
} rz_state_t;

char        *rz_run          (const char *cmd);
char        *rz_extract_field(const char *hay, const char *key,
                               char *out, size_t outlen);
rz_backend_t rz_detect_backend(void);

void rz_probe_wifi      (rz_wifi_t      *out);
void rz_probe_mobile    (rz_mobile_t    *out);
void rz_probe_dns       (rz_dns_t       *out);
void rz_probe_telephone (rz_telephone_t *out);
void rz_probe_sms       (rz_sms_t       *out);
void rz_probe_mms       (rz_mms_t       *out);
void rz_probe_ports     (rz_ports_t     *out);
void rz_probe_ethernet  (rz_ethernet_t  *out);
void rz_probe_uwb       (rz_uwb_t       *out);
void rz_probe_fm        (rz_fm_t        *out);
void rz_probe_usb       (rz_usb_t       *out);
void rz_probe_infrared  (rz_infrared_t  *out);
void rz_probe_bluetooth (rz_bluetooth_t *out);
void rz_probe_nfc       (rz_nfc_t       *out);
void rz_probe_xiaomi    (rz_xiaomi_t    *out);

void rz_snapshot    (rz_snapshot_t *out);
void rz_state_update(rz_state_t    *state);

int rz_emit_netstate (const rz_snapshot_t *snap);
int rz_emit_delta    (const rz_state_t *state, const char *layer,
                      const char *detail_json);
int rz_emit_anomaly  (const char *type, const char *detail_json);
int rz_emit_lifecycle(const char *event_type);

#endif /* RAHZERD_H */
