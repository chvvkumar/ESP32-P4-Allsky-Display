/*
 * On-demand device-health analyzer. See system_health.h.
 */

#include "system_health.h"
#include "system_monitor.h"
#include "system_crash_log.h"
#include "app_config.h"
#include "mqtt_ha.h"
#include "sensor_temp.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "sys_health";

/* Hardcoded thresholds (device-health.md, "Config fields read/written"). */
#define RSSI_WEAK        (-80)
#define RSSI_MODERATE    (-70)
#define DISC_HIGH        10
#define DISC_SOME        5
#define MQTT_RECON_HIGH  20
#define MQTT_RECON_SOME  10
#define TEMP_HIGH        80.0f
#define BOOT_HIGH_STATUS 50
#define BOOT_HIGH_REC    100
#define WDT_RESET_SOME   5
#define USAGE_HIGH_PCT   80.0f
#define BRIGHT_LOW       10
#define FRAG_HEAP_PCT    0.10f

/* ---- Event counters (RAM only) ----------------------------------------- */
static uint32_t s_disconnect_count;
static uint32_t s_mqtt_reconnect_count;
static uint32_t s_watchdog_resets;
static uint32_t s_roam_count;
static uint32_t s_roam_scan_count;
static uint32_t s_last_roam_time_ms;

static system_health_providers_t s_prov;

/* ---- Temperature sensor ------------------------------------------------ */
static float read_temperature_c(void)
{
    return sensor_temp_read_celsius();
}

/* ---- Provider helpers with fallbacks ----------------------------------- */
static bool prov_wifi_connected(void)
{
    return s_prov.wifi_connected ? s_prov.wifi_connected() : false;
}
static int prov_wifi_rssi(void)
{
    return s_prov.wifi_rssi ? s_prov.wifi_rssi() : 0;
}
static void prov_wifi_bssid(char *out, size_t len)
{
    if (s_prov.wifi_bssid) {
        s_prov.wifi_bssid(out, len);
    } else {
        snprintf(out, len, "00:00:00:00:00:00");
    }
}
static int prov_display_brightness(void)
{
    if (s_prov.display_brightness) {
        return s_prov.display_brightness();
    }
    return (int)app_config_get_def_bright();
}

/* ---- Lifecycle --------------------------------------------------------- */
void system_health_init(void)
{
    s_disconnect_count = 0;
    s_mqtt_reconnect_count = 0;
    s_watchdog_resets = 0;
    s_roam_count = 0;
    s_roam_scan_count = 0;
    s_last_roam_time_ms = 0;
    memset(&s_prov, 0, sizeof(s_prov));

    /* Wire the watchdog-reset counter that the legacy firmware never populated:
     * if the previous boot was a watchdog reset, count it once at startup. */
    if (crash_log_reset_was_watchdog()) {
        s_watchdog_resets = 1;
        ESP_LOGD(TAG, "watchdogResetCount=%lu (previous boot was a watchdog reset)",
                 (unsigned long)s_watchdog_resets);
    }
}

void system_health_set_providers(const system_health_providers_t *p)
{
    if (p) {
        s_prov = *p;
    } else {
        memset(&s_prov, 0, sizeof(s_prov));
    }
}

void system_health_record_network_disconnect(void)
{
    s_disconnect_count++;
    ESP_LOGD(TAG, "networkDisconnectCount=%lu", (unsigned long)s_disconnect_count);
}
void system_health_record_mqtt_reconnect(void)
{
    s_mqtt_reconnect_count++;
    ESP_LOGD(TAG, "mqttReconnectCount=%lu", (unsigned long)s_mqtt_reconnect_count);
}
void system_health_record_watchdog_reset(void)
{
    s_watchdog_resets++;
    ESP_LOGD(TAG, "watchdogResetCount=%lu", (unsigned long)s_watchdog_resets);
}
void system_health_record_roam(void)
{
    s_roam_count++;
    s_last_roam_time_ms = (uint32_t)(esp_timer_get_time() / 1000);
    ESP_LOGD(TAG, "roamCount=%lu", (unsigned long)s_roam_count);
}
void system_health_record_roam_scan(void)
{
    s_roam_scan_count++;
}

/* ---- Report model ------------------------------------------------------ */
typedef struct {
    health_level_t status;
    const char    *message;
} comp_result_t;

typedef struct {
    uint32_t timestamp_ms;

    /* memory */
    comp_result_t memory;
    uint32_t free_heap, total_heap, min_free_heap;
    float    heap_usage_pct;
    uint32_t free_psram, total_psram, min_free_psram;
    float    psram_usage_pct;

    /* network */
    comp_result_t network;
    bool     net_connected;
    int      rssi;
    uint32_t disconnect_count;
    char     bssid[24];
    uint32_t roam_count, roam_scan_count, last_roam_time_ms;

    /* mqtt */
    comp_result_t mqtt;
    bool     mqtt_connected;
    uint32_t mqtt_reconnect_count;

    /* system */
    comp_result_t system;
    bool     sys_healthy;
    uint32_t uptime_ms, boot_count;
    bool     last_boot_crash;
    float    temperature;
    uint32_t watchdog_resets;

    /* display */
    comp_result_t display;
    bool     disp_initialized;
    int      brightness;

    /* overall */
    health_level_t overall;
    const char    *overall_message;
    int      critical_issues;
    int      warnings;

    const char *recommendations[10];
    int         rec_count;
} health_report_t;

static const char *level_string(health_level_t l)
{
    switch (l) {
        case HEALTH_EXCELLENT: return "EXCELLENT";
        case HEALTH_GOOD:      return "GOOD";
        case HEALTH_WARNING:   return "WARNING";
        case HEALTH_CRITICAL:  return "CRITICAL";
        case HEALTH_FAILING:   return "FAILING";
        default:               return "UNKNOWN";
    }
}

static void analyze_memory(health_report_t *r)
{
    r->free_heap      = system_monitor_free_heap();
    r->total_heap     = system_monitor_total_heap();
    r->min_free_heap  = system_monitor_min_free_heap();
    r->free_psram     = system_monitor_free_psram();
    r->total_psram    = system_monitor_total_psram();
    r->min_free_psram = system_monitor_min_free_psram();

    r->heap_usage_pct  = r->total_heap  ? (100.0f - (float)r->free_heap  / r->total_heap  * 100.0f) : 0.0f;
    r->psram_usage_pct = r->total_psram ? (100.0f - (float)r->free_psram / r->total_psram * 100.0f) : 0.0f;

    uint32_t heap_th  = app_config_get_heap_thresh();
    uint32_t psram_th = app_config_get_psram_thresh();

    if (r->free_heap < heap_th || r->free_psram < psram_th) {
        r->memory.status = HEALTH_CRITICAL;
        r->memory.message = "Critical: Memory critically low!";
    } else if (r->min_free_heap < (uint32_t)(1.5f * heap_th) ||
               r->min_free_psram < (uint32_t)(1.5f * psram_th)) {
        r->memory.status = HEALTH_WARNING;
        r->memory.message = "Warning: Memory has been low during operation";
    } else if (r->heap_usage_pct > USAGE_HIGH_PCT || r->psram_usage_pct > USAGE_HIGH_PCT) {
        r->memory.status = HEALTH_GOOD;
        r->memory.message = "Good: Memory usage is high but stable";
    } else {
        r->memory.status = HEALTH_EXCELLENT;
        r->memory.message = "Excellent: Memory levels optimal";
    }
}

static void analyze_network(health_report_t *r)
{
    r->net_connected   = prov_wifi_connected();
    r->rssi            = prov_wifi_rssi();
    r->disconnect_count = s_disconnect_count;
    prov_wifi_bssid(r->bssid, sizeof(r->bssid));
    r->roam_count      = s_roam_count;
    r->roam_scan_count = s_roam_scan_count;
    r->last_roam_time_ms = s_last_roam_time_ms;

    if (!r->net_connected) {
        r->network.status = HEALTH_FAILING;
        r->network.message = "Critical: WiFi disconnected";
    } else if (r->rssi < RSSI_WEAK) {
        r->network.status = HEALTH_WARNING;
        r->network.message = "Warning: Weak WiFi signal (below -80 dBm)";
    } else if (r->rssi < RSSI_MODERATE) {
        r->network.status = HEALTH_GOOD;
        r->network.message = "Good: WiFi signal moderate";
    } else if (r->disconnect_count > DISC_HIGH) {
        r->network.status = HEALTH_WARNING;
        r->network.message = "Warning: Frequent WiFi disconnections detected";
    } else if (r->disconnect_count > DISC_SOME) {
        r->network.status = HEALTH_GOOD;
        r->network.message = "Good: Some WiFi disconnections, but stable";
    } else {
        r->network.status = HEALTH_EXCELLENT;
        r->network.message = "Excellent: WiFi stable with strong signal";
    }
}

static bool mqtt_server_configured(void)
{
    char srv[64];
    app_config_get_mqtt_server(srv, sizeof(srv));
    return srv[0] != '\0';
}

static void analyze_mqtt(health_report_t *r)
{
    r->mqtt_connected = mqtt_ha_is_connected();
    r->mqtt_reconnect_count = s_mqtt_reconnect_count;

    if (!mqtt_server_configured()) {
        r->mqtt.status = HEALTH_EXCELLENT;
        r->mqtt.message = "Not configured (optional)";
        return;
    }
    if (!r->mqtt_connected) {
        r->mqtt.status = HEALTH_CRITICAL;
        r->mqtt.message = "Critical: MQTT disconnected (server configured)";
    } else if (r->mqtt_reconnect_count > MQTT_RECON_HIGH) {
        r->mqtt.status = HEALTH_WARNING;
        r->mqtt.message = "Warning: Frequent MQTT reconnections";
    } else if (r->mqtt_reconnect_count > MQTT_RECON_SOME) {
        r->mqtt.status = HEALTH_GOOD;
        r->mqtt.message = "Good: MQTT connected with some reconnects";
    } else {
        r->mqtt.status = HEALTH_EXCELLENT;
        r->mqtt.message = "Excellent: MQTT stable";
    }
}

static void analyze_system(health_report_t *r)
{
    r->sys_healthy      = system_monitor_is_healthy();
    r->uptime_ms        = (uint32_t)(esp_timer_get_time() / 1000);
    r->boot_count       = crash_log_boot_count();
    r->last_boot_crash  = crash_log_last_boot_was_crash();
    r->temperature      = read_temperature_c();
    r->watchdog_resets  = s_watchdog_resets;

    if (!r->sys_healthy) {
        r->system.status = HEALTH_CRITICAL;
        r->system.message = "Critical: System health check failed";
    } else if (r->last_boot_crash) {
        r->system.status = HEALTH_WARNING;
        r->system.message = "Warning: Previous boot was a crash";
    } else if (!isnan(r->temperature) && r->temperature > TEMP_HIGH) {
        r->system.status = HEALTH_WARNING;
        r->system.message = "Warning: High CPU temperature";
    } else if (r->boot_count > BOOT_HIGH_STATUS) {
        r->system.status = HEALTH_GOOD;
        r->system.message = "Good: High boot count (possible instability history)";
    } else if (r->watchdog_resets > WDT_RESET_SOME) {
        r->system.status = HEALTH_GOOD;
        r->system.message = "Good: Some watchdog resets detected";
    } else {
        r->system.status = HEALTH_EXCELLENT;
        r->system.message = "Excellent: System stable";
    }
}

static void analyze_display(health_report_t *r)
{
    r->disp_initialized = true;
    r->brightness = prov_display_brightness();

    if (!r->disp_initialized) {
        r->display.status = HEALTH_FAILING;
        r->display.message = "Critical: Display not initialized";
    } else if (r->brightness < BRIGHT_LOW) {
        r->display.status = HEALTH_WARNING;
        r->display.message = "Warning: Display brightness very low";
    } else {
        r->display.status = HEALTH_EXCELLENT;
        r->display.message = "Excellent: Display operating normally";
    }
}

static void rec_append(health_report_t *r, const char *text)
{
    if (r->rec_count < (int)(sizeof(r->recommendations) / sizeof(r->recommendations[0]))) {
        r->recommendations[r->rec_count++] = text;
    }
}

static void build_recommendations(health_report_t *r)
{
    if (r->memory.status >= HEALTH_WARNING) {
        rec_append(r, "Reduce image buffer sizes or reduce max scale factor in config.h");
    }
    if (r->total_heap && r->min_free_heap < (uint32_t)(FRAG_HEAP_PCT * r->total_heap)) {
        rec_append(r, "Memory fragmentation possible - consider reducing update frequency");
    }
    if (r->network.status >= HEALTH_WARNING && r->net_connected) {
        rec_append(r, "Improve WiFi signal by relocating device or access point");
    }
    if (!r->net_connected) {
        rec_append(r, "Check WiFi credentials and router availability");
    }
    if (r->disconnect_count > DISC_HIGH) {
        rec_append(r, "Investigate network stability - check router logs");
    }
    if (r->mqtt.status >= HEALTH_WARNING && mqtt_server_configured()) {
        rec_append(r, "Check MQTT broker availability and credentials");
    }
    if (r->last_boot_crash) {
        rec_append(r, "Check crash logs for root cause - see /console page");
    }
    if (!isnan(r->temperature) && r->temperature > TEMP_HIGH) {
        rec_append(r, "Improve device ventilation to reduce CPU temperature");
    }
    if (r->boot_count > BOOT_HIGH_REC) {
        rec_append(r, "High boot count may indicate power issues or instability");
    }
    if (r->mqtt_reconnect_count > MQTT_RECON_HIGH) {
        rec_append(r, "Increase MQTT reconnect interval or check broker stability");
    }
}

static health_level_t worst(health_level_t a, health_level_t b)
{
    return a > b ? a : b;
}

static void count_issue(health_report_t *r, health_level_t s)
{
    if (s >= HEALTH_CRITICAL) {
        r->critical_issues++;
    } else if (s >= HEALTH_WARNING) {
        r->warnings++;
    }
}

static void build_report(health_report_t *r)
{
    memset(r, 0, sizeof(*r));
    r->timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);

    analyze_memory(r);
    analyze_network(r);
    analyze_mqtt(r);
    analyze_system(r);
    analyze_display(r);

    count_issue(r, r->memory.status);
    count_issue(r, r->network.status);
    count_issue(r, r->mqtt.status);
    count_issue(r, r->system.status);
    count_issue(r, r->display.status);

    r->overall = HEALTH_EXCELLENT;
    r->overall = worst(r->overall, r->memory.status);
    r->overall = worst(r->overall, r->network.status);
    r->overall = worst(r->overall, r->mqtt.status);
    r->overall = worst(r->overall, r->system.status);
    r->overall = worst(r->overall, r->display.status);

    switch (r->overall) {
        case HEALTH_EXCELLENT: r->overall_message = "Device is operating optimally"; break;
        case HEALTH_GOOD:      r->overall_message = "Device is functional with minor issues"; break;
        case HEALTH_WARNING:   r->overall_message = "Device has issues that need attention"; break;
        case HEALTH_CRITICAL:  r->overall_message = "Device has critical issues requiring immediate action"; break;
        case HEALTH_FAILING:   r->overall_message = "Device is failing or unstable"; break;
        default:               r->overall_message = "Unknown"; break;
    }

    build_recommendations(r);
}

/* ---- JSON: force exactly one decimal place on the float fields ---------- */
static void add_float1(cJSON *obj, const char *key, float v)
{
    if (isnan(v) || isinf(v)) {
        v = 0.0f;
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", v);
    cJSON_AddRawToObject(obj, key, buf);
}

struct cJSON *system_health_build_json(void)
{
    health_report_t r;
    build_report(&r);

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    cJSON *overall = cJSON_AddObjectToObject(root, "overall");
    cJSON_AddStringToObject(overall, "status", level_string(r.overall));
    cJSON_AddStringToObject(overall, "message", r.overall_message);
    cJSON_AddNumberToObject(overall, "critical_issues", r.critical_issues);
    cJSON_AddNumberToObject(overall, "warnings", r.warnings);
    cJSON_AddNumberToObject(overall, "timestamp", r.timestamp_ms);

    cJSON *mem = cJSON_AddObjectToObject(root, "memory");
    cJSON_AddStringToObject(mem, "status", level_string(r.memory.status));
    cJSON_AddStringToObject(mem, "message", r.memory.message);
    cJSON_AddNumberToObject(mem, "free_heap", r.free_heap);
    cJSON_AddNumberToObject(mem, "total_heap", r.total_heap);
    cJSON_AddNumberToObject(mem, "min_free_heap", r.min_free_heap);
    add_float1(mem, "heap_usage_percent", r.heap_usage_pct);
    cJSON_AddNumberToObject(mem, "free_psram", r.free_psram);
    cJSON_AddNumberToObject(mem, "total_psram", r.total_psram);
    cJSON_AddNumberToObject(mem, "min_free_psram", r.min_free_psram);
    add_float1(mem, "psram_usage_percent", r.psram_usage_pct);

    cJSON *net = cJSON_AddObjectToObject(root, "network");
    cJSON_AddStringToObject(net, "status", level_string(r.network.status));
    cJSON_AddStringToObject(net, "message", r.network.message);
    cJSON_AddBoolToObject(net, "connected", r.net_connected);
    cJSON_AddNumberToObject(net, "rssi", r.rssi);
    cJSON_AddNumberToObject(net, "disconnect_count", r.disconnect_count);
    cJSON_AddStringToObject(net, "bssid", r.bssid);
    cJSON_AddNumberToObject(net, "roam_count", r.roam_count);
    cJSON_AddNumberToObject(net, "roam_scan_count", r.roam_scan_count);
    cJSON_AddNumberToObject(net, "last_roam_time_ms", r.last_roam_time_ms);

    cJSON *mq = cJSON_AddObjectToObject(root, "mqtt");
    cJSON_AddStringToObject(mq, "status", level_string(r.mqtt.status));
    cJSON_AddStringToObject(mq, "message", r.mqtt.message);
    cJSON_AddBoolToObject(mq, "connected", r.mqtt_connected);
    cJSON_AddNumberToObject(mq, "reconnect_count", r.mqtt_reconnect_count);

    cJSON *sys = cJSON_AddObjectToObject(root, "system");
    cJSON_AddStringToObject(sys, "status", level_string(r.system.status));
    cJSON_AddStringToObject(sys, "message", r.system.message);
    cJSON_AddBoolToObject(sys, "healthy", r.sys_healthy);
    cJSON_AddNumberToObject(sys, "uptime_ms", r.uptime_ms);
    cJSON_AddNumberToObject(sys, "boot_count", r.boot_count);
    cJSON_AddBoolToObject(sys, "last_boot_crash", r.last_boot_crash);
    add_float1(sys, "temperature", r.temperature);
    cJSON_AddNumberToObject(sys, "watchdog_resets", r.watchdog_resets);

    cJSON *disp = cJSON_AddObjectToObject(root, "display");
    cJSON_AddStringToObject(disp, "status", level_string(r.display.status));
    cJSON_AddStringToObject(disp, "message", r.display.message);
    cJSON_AddBoolToObject(disp, "initialized", r.disp_initialized);
    cJSON_AddNumberToObject(disp, "brightness", r.brightness);

    cJSON *recs = cJSON_AddArrayToObject(root, "recommendations");
    for (int i = 0; i < r.rec_count; i++) {
        cJSON_AddItemToArray(recs, cJSON_CreateString(r.recommendations[i]));
    }

    return root;
}

/* ---- Text report ------------------------------------------------------- */
size_t system_health_text_report(char *out, size_t len)
{
    if (!out || len == 0) {
        return 0;
    }
    health_report_t r;
    build_report(&r);

    float temp = isnan(r.temperature) ? 0.0f : r.temperature;
    int off = 0;
    #define APP(...) do { \
        if ((size_t)off < len) { \
            int _n = snprintf(out + off, len - off, __VA_ARGS__); \
            if (_n > 0) off += _n; \
        } \
    } while (0)

    APP("========== DEVICE HEALTH ==========\n");
    APP("Overall Status: %s - %s\n", level_string(r.overall), r.overall_message);
    APP("Critical Issues: %d | Warnings: %d\n", r.critical_issues, r.warnings);
    APP("[MEMORY]  %s - %s\n", level_string(r.memory.status), r.memory.message);
    APP("  heap  %lu/%lu B (%.1f%%) min %lu\n",
        (unsigned long)(r.total_heap - r.free_heap), (unsigned long)r.total_heap,
        r.heap_usage_pct, (unsigned long)r.min_free_heap);
    APP("  psram %lu/%lu B (%.1f%%) min %lu\n",
        (unsigned long)(r.total_psram - r.free_psram), (unsigned long)r.total_psram,
        r.psram_usage_pct, (unsigned long)r.min_free_psram);
    APP("[NETWORK] %s - %s\n", level_string(r.network.status), r.network.message);
    APP("  connected=%s rssi=%d dBm disconnects=%lu bssid=%s roams=%lu scans=%lu\n",
        r.net_connected ? "yes" : "no", r.rssi, (unsigned long)r.disconnect_count,
        r.bssid, (unsigned long)r.roam_count, (unsigned long)r.roam_scan_count);
    APP("[MQTT]    %s - %s\n", level_string(r.mqtt.status), r.mqtt.message);
    APP("  connected=%s reconnects=%lu\n",
        r.mqtt_connected ? "yes" : "no", (unsigned long)r.mqtt_reconnect_count);
    APP("[SYSTEM]  %s - %s\n", level_string(r.system.status), r.system.message);
    APP("  uptime=%.1f h boot=%lu lastCrash=%s temp=%.1f C wdtResets=%lu\n",
        r.uptime_ms / 3600000.0f, (unsigned long)r.boot_count,
        r.last_boot_crash ? "yes" : "no", temp, (unsigned long)r.watchdog_resets);
    APP("[DISPLAY] %s - %s\n", level_string(r.display.status), r.display.message);
    APP("  brightness=%d%%\n", r.brightness);
    if (r.rec_count > 0) {
        APP("RECOMMENDATIONS:\n");
        for (int i = 0; i < r.rec_count; i++) {
            APP("  %d. %s\n", i + 1, r.recommendations[i]);
        }
    }
    APP("===================================\n");
    #undef APP

    if ((size_t)off >= len) {
        off = (int)len - 1;
    }
    out[off] = '\0';
    return (size_t)off;
}

float system_health_temperature_c(void)
{
    return read_temperature_c();
}

/* ---- Quick predicates -------------------------------------------------- */
bool system_health_memory_ok(void)
{
    health_report_t r;
    memset(&r, 0, sizeof(r));
    analyze_memory(&r);
    return r.memory.status <= HEALTH_GOOD;
}

bool system_health_system_ok(void)
{
    health_report_t r;
    memset(&r, 0, sizeof(r));
    analyze_system(&r);
    return r.system.status <= HEALTH_GOOD;
}

health_level_t system_health_quick_overall(void)
{
    health_report_t r;
    build_report(&r);
    return r.overall;
}
