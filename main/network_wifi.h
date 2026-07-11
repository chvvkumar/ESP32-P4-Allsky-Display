#pragma once

/**
 * @file network_wifi.h
 * @brief Station-mode WiFi manager: bring-up, non-blocking connect/reconnect
 * state machine with exponential backoff, AP roaming, scan API, and status.
 *
 * The C6 radio is reached over ESP-Hosted (SDIO) via esp_wifi_remote, which
 * mirrors the esp_wifi API. All state transitions run from network_wifi_update(),
 * called cooperatively from the main loop (~50 ms cadence); the ESP event
 * handlers only latch link/scan flags, they never drive transitions. No call
 * here blocks longer than the watchdog allows except network_wifi_scan_json(),
 * which is intended to run on the web server task, not the main loop.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bit set in the network event group while the station has an IP. */
#define NETWORK_WIFI_CONNECTED_BIT  BIT0

typedef struct {
    bool connected;
    char ssid[33];
    char ip[16];
    char gateway[16];
    char dns[16];
    char mac[18];
    char bssid[18];
    char hostname[64];
    int  rssi;
} network_wifi_status_t;

/**
 * Initialize the TCP/IP stack, default event loop, the STA netif (set as the
 * system default netif so unbound UDP egresses it), esp_wifi in station mode,
 * register event handlers, apply the derived hostname, and start the radio.
 * Loads wifi_ssid / wifi_pwd from config. Does NOT begin connecting; the caller
 * waits ~500 ms for radio readiness, then calls network_wifi_connect_start().
 */
esp_err_t network_wifi_init(void);

/**
 * Begin the connect state machine. Reports an error and does nothing if the
 * stored SSID is empty ("WiFi SSID not configured").
 */
esp_err_t network_wifi_connect_start(void);

/** Non-blocking state-machine tick. Call every main-loop iteration. */
void network_wifi_update(void);

/** True when the internal connected flag is set AND the driver reports a link
 * with an IP. Consumers gate all network activity on this. */
bool network_wifi_is_connected(void);

/** Fill @p out with the current status snapshot. Safe to call any time. */
void network_wifi_get_status(network_wifi_status_t *out);

/** The derived DHCP/mDNS hostname (sanitized from device_name). */
const char *network_wifi_get_hostname(void);

/**
 * Blocking WiFi scan producing the web-UI JSON described in network.md 7.3 into
 * @p buf. Intended for the web server task. Returns the network count (>=0),
 * -1 on scan failure, or -2 when rate-limited (<10 s since the last scan).
 */
int network_wifi_scan_json(char *buf, size_t buf_len);

/** Event group other tasks wait on; NETWORK_WIFI_CONNECTED_BIT tracks link+IP. */
EventGroupHandle_t network_wifi_event_group(void);

#ifdef __cplusplus
}
#endif
