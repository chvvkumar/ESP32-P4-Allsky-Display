#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Integration glue between subsystems that the top-level app_main wires together.
 * The strong web-integration hook definitions live in integration_hooks.c; the
 * functions declared here are the adapters app_main installs into subsystem hook
 * structs where a direct 1:1 signature match is not available.
 */

/* Health provider adapters (network status -> device-health analyzer). */
int  integ_wifi_rssi(void);
void integ_wifi_bssid(char *out, size_t len);

/* MQTT WiFi-signal provider: station RSSI in dBm into *out; false if the radio
 * is not associated (matches the mqtt_ha get_rssi contract, bool(int8_t*)). */
bool integ_mqtt_rssi(int8_t *out);

/* MQTT: currentImageIndex changed externally; force the pipeline to re-fetch. */
void integ_image_index_changed(int new_index);

/* Serial 'X': stop and restart the web server. */
void integ_restart_web_server(void);

/* Serial diagnostic report providers (write a text block into out/len). */
void integ_serial_web_status(char *out, size_t len);    /* 'X' web/auth status */
void integ_serial_network_info(char *out, size_t len);  /* 'I' network block   */
void integ_serial_ppa_info(char *out, size_t len);      /* 'P' PPA block       */

/* Touch single tap: advance to the next image, suppressed on the moon page. */
void integ_single_tap(void);

/* Moon interactive drag sinks: upscale the small interactive frame to the panel
 * and present it; settle triggers a crisp full-resolution resting re-render. */
void integ_moon_blit(const uint16_t *src, int src_w, int src_h);
void integ_moon_settle(void);

#ifdef __cplusplus
}
#endif
