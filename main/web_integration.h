#pragma once

/*
 * Integration seam between the web server / UI subsystem and the rest of the
 * firmware. Every function here is a hook the web layer calls into. Default weak
 * implementations live in web_integration_stubs.c so this subsystem builds and
 * links standalone. The integration phase provides strong definitions (in
 * main.c or a bridge translation unit) that forward to the real image pipeline,
 * display manager, network manager, MQTT client, crash logger and log ring.
 *
 * A strong definition anywhere in the link overrides the weak default, so wiring
 * a subsystem is: implement its hook. Forget one, and the server degrades to a
 * safe no-op / neutral value instead of failing to link.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Display / rendering ------------------------------------------------- */

/* Apply a brightness percentage (0..100) to the backlight immediately. */
void web_hook_apply_brightness(int percent);

/* Pause / resume panel refresh (used around screenshot capture). */
void web_hook_display_pause(bool pause);

/* Show a short status string on the panel (mode messages, restart notice). */
void web_hook_display_message(const char *msg);

/* Full-screen OTA progress takeover. progress<0 hides; 0..100 shows percent. */
void web_hook_ota_screen(int progress, const char *state_text);

/* ---- Image pipeline ------------------------------------------------------ */

/* Live displayed source index (may differ from the persisted current index
 * while tuning). Returns 0 when unknown. */
int web_hook_image_current_index(void);

/* Queue an immediate re-download of the current image. */
void web_hook_image_queue_download(void);

/* Advance to the next enabled image, reset the cycle timer, queue a download. */
void web_hook_image_next(void);

/* Re-render the current frame with the active transform / defaults. */
void web_hook_image_rerender(void);

/* Enter/leave "paused for editing" (suspends auto-cycling). */
void web_hook_image_set_editing_paused(bool paused);

/* Hold a specific source on screen for tuning. persist_index=true also writes it
 * as the startup/current index; false is runtime-only (tune mode). */
void web_hook_image_tune(int index, bool persist_index);

/* Leave tune mode and resume cycling. */
void web_hook_image_tune_stop(void);

/* Notify that cycling-mode state changed (mode message + queued download). */
void web_hook_image_cycling_changed(bool cycling_enabled);

/* ---- Network ------------------------------------------------------------- */

typedef struct {
    bool     connected;
    char     ssid[33];
    char     ip[16];
    char     gateway[16];
    char     dns[16];
    char     mac[18];
    char     bssid[18];
    char     hostname[64];
    int      rssi;
} web_net_status_t;

void web_hook_net_status(web_net_status_t *out);

/* Blocking WiFi scan for the /api/wifi-scan endpoint. Writes a JSON array of
 * network objects into buf (see web-server.md 8.6). Returns:
 *   >0  number of networks (JSON written)
 *    0  no networks (empty array written)
 *   -1  scan failure
 *   -2  rate limited (caller returns 429) */
int web_hook_net_wifi_scan(char *buf, size_t buf_len);

/* Trigger an NTP / time re-sync after time settings change. */
void web_hook_net_resync_time(void);

/* ---- Captive provisioning portal (network manager owns AP/DNS/test) ------- */

/* Kick off an asynchronous provisioning-mode WiFi scan. Idempotent. */
void web_hook_captive_scan_start(void);

/* Write the captive scan result JSON into buf:
 *   {"status":"scanning"} while a scan is in progress, else
 *   {"status":"complete","networks":[{"ssid","rssi","encrypted"},...]}
 * deduped by SSID, sorted by descending RSSI. Returns bytes written. */
size_t web_hook_captive_scan_results(char *buf, size_t buf_len);

/* Save SSID/password + provisioned flag, attempt a test join in AP+STA mode,
 * then reboot (kept credentials regardless of outcome). Returns after scheduling
 * the async test+reboot; the HTTP response is sent before the reboot fires. */
void web_hook_captive_connect(const char *ssid, const char *password);

/* ---- MQTT ---------------------------------------------------------------- */

bool web_hook_mqtt_connected(void);
void web_hook_mqtt_publish_state(void);

/* Notify MQTT/HA discovery that the image source list changed (rebuild the
 * source select entity). No-op when MQTT is not wired. */
void web_hook_mqtt_sources_changed(void);

/* ---- HA REST brightness -------------------------------------------------- */

/* Re-apply the last polled ambient brightness. Returns true if HA REST control
 * is active and a value was applied. */
bool web_hook_ha_rest_reapply_brightness(void);

/* ---- System health ------------------------------------------------------- */

/* Write the device-health JSON report (see device-health spec) into buf.
 * Returns bytes written (excluding NUL). */
size_t web_hook_health_report_json(char *buf, size_t buf_len);

/* Coarse healthy flag for status badges / info JSON. */
bool web_hook_system_healthy(void);

/* Temperature in Celsius (NaN-safe: returns a plausible value). */
float web_hook_system_temperature_c(void);

/* ---- Crash / boot log ---------------------------------------------------- */

/* Copy preserved boot/crash log bytes (oldest-first) into dst; returns count. */
size_t web_hook_bootlog_read(size_t offset, char *dst, size_t max_len);
size_t web_hook_bootlog_size(void);

/* Drain up to max_len bytes of NEW log output emitted since the previous call
 * (for the live WebSocket console). Returns bytes copied; 0 when no new output.
 * The system log subsystem implements this over its capture ring. */
size_t web_hook_logstream_read(char *dst, size_t max_len);

/* Clear crash/boot logs in RTC and NVS. */
void web_hook_crash_clear(void);

/* Persist crash-log state right before an intentional reboot. */
void web_hook_crash_save(void);

#ifdef __cplusplus
}
#endif
