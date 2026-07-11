#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AllSky configuration web server (port 8080) + live-log WebSocket (/ws/logs) +
 * REST API + screenshot + OTA. Provisioning captive portal is a separate server
 * started only in AP mode (web_captive_start/stop).
 *
 * Integration order in app_main (after WiFi has a station connection):
 *   1. web_ota_mark_valid_after_boot()   -- once, early, to confirm a fresh OTA image
 *   2. web_server_start()                -- idempotent; safe to call again to restart
 */

/* Start the main HTTP + WebSocket server on port 8080. Idempotent: a second call
 * while running returns ESP_OK without side effects. */
esp_err_t web_server_start(void);

/* Tear down the main server (and its WebSocket). */
void web_server_stop(void);

/* True while the main server is running. */
bool web_server_is_running(void);

/* Enable/disable the default-deny auth gate at runtime and set the admin
 * password (may be NULL to leave unchanged). Disabled by default. */
void web_auth_configure(bool enabled, const char *password);
bool web_auth_is_enabled(void);

/* Confirm the running OTA image as valid so the bootloader stops rolling back.
 * Call once early at boot after the app is known-good (WiFi + config loaded). */
void web_ota_mark_valid_after_boot(void);

/* True while a firmware OTA transfer is in progress. Consumed by the image
 * pipeline and MQTT subsystem to suspend heavy activity during an update. */
bool web_ota_in_progress(void);

/* ---- Captive provisioning portal (port 80), started by the network manager
 * while in AP provisioning mode. Serves the setup page, WiFi scan, and the
 * credential-save+test+reboot join flow. ------------------------------------ */
esp_err_t web_captive_start(void);
void      web_captive_stop(void);

#ifdef __cplusplus
}
#endif
