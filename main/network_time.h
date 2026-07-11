#pragma once

/**
 * @file network_time.h
 * @brief Two-stage time synchronization: best-effort SNTP (primary) plus an
 * authoritative HTTP-Date fallback fetched from the configured image server.
 *
 * SNTP is primary because the default-netif fix (network_wifi sets the STA netif
 * as default) makes unbound UDP route correctly. The HTTP-Date path is the
 * fallback for LANs without internet, and targets the configured image server's
 * Date header (not a public endpoint), so an isolated deployment still gets a
 * valid clock. All work is non-blocking off network_time_update(), except the
 * HTTP HEAD attempt itself, which is watchdog-bracketed.
 */

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Apply the configured POSIX TZ string to the C library. Call once at boot. */
esp_err_t network_time_init(void);

/** Called by the WiFi manager on every successful (re)connect: kicks the SNTP
 * attempt and schedules the first HTTP-Date sync 8 s out. */
void network_time_on_connected(void);

/** Non-blocking tick: polls SNTP validity and performs a due HTTP-Date sync. */
void network_time_update(void);

/** Force an immediate re-sync (SNTP restart + HTTP-Date due now). Used after a
 * time-settings change from the web UI. */
void network_time_resync(void);

/** True when local calendar time is obtainable and the year is > 2020. */
bool network_time_is_valid(void);

/** True while an SNTP validity poll window is active (roaming scans are skipped
 * while this is true, per network.md 4). */
bool network_time_sntp_in_progress(void);

#ifdef __cplusplus
}
#endif
