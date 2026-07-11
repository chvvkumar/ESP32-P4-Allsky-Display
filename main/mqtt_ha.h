#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MQTT + Home Assistant discovery subsystem.
 *
 * Maintains one broker connection with exponential backoff and an LWT
 * availability contract, publishes a 28-entity Home Assistant discovery set
 * (non-blocking, one entity per service pass), subscribes to a single wildcard
 * command filter, and publishes periodic sensor state plus a 30 s availability
 * heartbeat.
 *
 * The MQTT event callback only parses and enqueues inbound commands; an internal
 * service task applies them under the config API and persists. No NVS write or
 * backlight call ever runs in the mqtt event context.
 *
 * Integration owns the hook callbacks below (thin interfaces to the input,
 * image-pipeline, and system subsystems) so this subsystem has no direct
 * dependency on their internals.
 */

/* Integration-supplied callbacks. Any field may be NULL; documented fallbacks
 * apply. Pointers must remain valid for the life of the subsystem. */
typedef struct {
    /* Apply backlight brightness live, 0..100. Used by the MQTT brightness
     * command and the HA REST light-sensor loop. NULL: brightness is only
     * stored to config, never driven to hardware. */
    void (*apply_brightness)(int pct);
    /* Read the current backlight brightness 0..100 for state/sensor publishes.
     * NULL: falls back to the persisted defaultBrightness. */
    int  (*get_brightness)(void);
    /* Notify the image pipeline that currentImageIndex changed (restart the
     * per-image cycle timer and force an immediate download). NULL: no-op. */
    void (*image_index_changed)(int new_index);
    /* Persist crash/boot forensics before an MQTT-requested reboot. NULL: no-op. */
    void (*before_reboot)(void);
    /* Record a reconnect attempt with the device-health monitor. NULL: no-op. */
    void (*on_reconnect_attempt)(void);
    /* Station RSSI in dBm into *out; return true on success. NULL: this
     * subsystem calls esp_wifi_sta_get_ap_info() itself. */
    bool (*get_rssi)(int8_t *out);
    /* True while an OTA transfer is active; MQTT traffic is suspended.
     * NULL: assumed idle. */
    bool (*ota_in_progress)(void);
    /* True while the WiFi station is connected. Reconnect attempts are gated on
     * this. NULL: assumed connected. */
    bool (*wifi_connected)(void);
} mqtt_ha_hooks_t;

/* Register hooks, precompute the device ID and topic strings, create the command
 * queue and the internal service task (parked until mqtt_ha_start). Call once
 * early in boot after nvs/app_config init. `hooks` may be NULL. */
esp_err_t mqtt_ha_init(const mqtt_ha_hooks_t *hooks);

/* Start (or restart) the broker connection using the stored server/port. No-op
 * with a log if the broker field is empty. Safe to call after WiFi comes up. */
esp_err_t mqtt_ha_start(void);

/* Publish offline availability (if connected) and tear down the client. */
void mqtt_ha_stop(void);

/* True while the broker connection is established. */
bool mqtt_ha_is_connected(void);

/* Publish the full entity state snapshot followed by all sensors. Safe to call
 * from any task; no-op unless connected and discovery is enabled. Invoked by the
 * web brightness endpoint while MQTT auto-brightness mode is active. */
void mqtt_ha_publish_state(void);

/* Request a discovery re-run (e.g. after the image source list changed so the
 * select option list must be rebuilt). Idempotent; takes effect on the next
 * service pass while connected. */
void mqtt_ha_notify_sources_changed(void);

/* Force a reconnect that also re-runs discovery (serial 'M' recovery path). */
void mqtt_ha_force_reconnect(void);

/* Write a human-readable connection-info dump into `out` (serial diagnostics). */
void mqtt_ha_connection_info(char *out, size_t len);

/* ---- HA REST ambient-light brightness client (input-display.md section 5) --- */

/* Start the HA REST light-sensor polling task. Idempotent; the task itself gates
 * on the useHaRestControl config flag and WiFi state each cycle, so it may be
 * started unconditionally at boot. */
esp_err_t mqtt_ha_rest_start(void);

/* Latest cached HA REST reading. Returns false if no successful poll yet.
 * `lux` and `brightness` may each be NULL. For the web status API. */
bool mqtt_ha_rest_last(float *lux, int *brightness);

#ifdef __cplusplus
}
#endif
