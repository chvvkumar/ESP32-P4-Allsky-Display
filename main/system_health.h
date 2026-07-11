#pragma once

/*
 * On-demand device-health analyzer.
 *
 * Owns no task or timer. On request it samples five components (memory, network,
 * MQTT, system, display), assigns each an ordered status level, derives the
 * worst-case overall status, counts issues, generates recommendations, and emits
 * either a cJSON document (for GET /api/health, wired by the web team) or a
 * formatted text report (for the serial `health`/`G` command).
 *
 * It also holds five in-RAM event counters that other subsystems increment
 * through the record_* entry points. All counters are zero at boot and never
 * persist. Live network/display values it cannot read itself are supplied by
 * providers registered at init.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* cJSON is opaque here to keep this header light; the .c includes cJSON.h. */
struct cJSON;

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEALTH_EXCELLENT = 0,
    HEALTH_GOOD      = 1,
    HEALTH_WARNING   = 2,
    HEALTH_CRITICAL  = 3,
    HEALTH_FAILING   = 4,
} health_level_t;

/* Live values this subsystem cannot sample on its own. Any field may be NULL;
 * documented fallbacks apply (disconnected / zeroed). */
typedef struct {
    bool (*wifi_connected)(void);              /* NULL -> false */
    int  (*wifi_rssi)(void);                   /* dBm; NULL -> 0 */
    void (*wifi_bssid)(char *out, size_t len); /* "AA:BB:.."; NULL -> zero MAC */
    int  (*display_brightness)(void);          /* 0..100; NULL -> configured default */
} system_health_providers_t;

/* Seed the watchdog-reset counter from the crash logger (if the previous boot was
 * a watchdog reset) and clear all other counters. Call once at boot after
 * crash_log_init(). */
void system_health_init(void);

/* Register live-value providers. `p` is copied; pass NULL to clear. */
void system_health_set_providers(const system_health_providers_t *p);

/* Event counters (see device-health.md 2). Each logs the new value at debug level
 * where the spec requires it. */
void system_health_record_network_disconnect(void);
void system_health_record_mqtt_reconnect(void);
void system_health_record_watchdog_reset(void);
void system_health_record_roam(void);
void system_health_record_roam_scan(void);

/* Build a fresh report and serialize it as the /api/health contract object.
 * Returns a newly allocated cJSON object the caller owns (cJSON_Delete), or NULL
 * on allocation failure. */
struct cJSON *system_health_build_json(void);

/* Build a fresh report and format it as the bordered text block. Writes up to
 * len-1 bytes (NUL-terminated) and returns the number of bytes written. */
size_t system_health_text_report(char *out, size_t len);

/* On-die temperature in Celsius. Returns NAN if the sensor is unavailable. */
float system_health_temperature_c(void);

/* Lightweight predicates that evaluate the analyzers without building strings. */
bool           system_health_memory_ok(void);   /* memory GOOD or better */
bool           system_health_system_ok(void);   /* system GOOD or better */
health_level_t system_health_quick_overall(void);

#ifdef __cplusplus
}
#endif
