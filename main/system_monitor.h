#pragma once

/*
 * Periodic system monitor: memory health sampling, stack high-water logging,
 * serial flush, and the system-healthy flag.
 *
 * It owns no task of its own. system_monitor_update() is called once per main-loop
 * iteration and internally rate-limits its three cadences (memory 30 s, stack
 * 60 s, serial flush 5 s) by comparing uptime against stored timestamps, matching
 * the loop-driven periodicity model of the rest of the firmware.
 *
 * The session minimum free-memory figures and the healthy flag produced here are
 * consumed by the device-health analyzer (system_health) and by the MQTT sensor
 * publishes. When memory stays below the configured thresholds for several
 * consecutive checks the monitor escalates to a controlled reboot.
 */

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Read thresholds from config, seed the session minimums from the current free
 * memory, and reset cadence timers. Call once at boot after app_config_init(). */
esp_err_t system_monitor_init(void);

/* Advance the three cadences. Call every main-loop iteration; cheap when no
 * cadence is due. */
void system_monitor_update(void);

/* True while both free heap and free PSRAM are at or above their configured
 * critical thresholds. Cleared/set only at the 30 s memory check. */
bool system_monitor_is_healthy(void);

/* Live and session-minimum free-memory figures (bytes) and totals, for the health
 * report and MQTT sensors. */
uint32_t system_monitor_free_heap(void);
uint32_t system_monitor_min_free_heap(void);
uint32_t system_monitor_total_heap(void);
uint32_t system_monitor_free_psram(void);
uint32_t system_monitor_min_free_psram(void);
uint32_t system_monitor_total_psram(void);

#ifdef __cplusplus
}
#endif
