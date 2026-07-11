#pragma once

/**
 * @file network_poll.h
 * @brief Shared poll-loop skeleton for independent background pollers.
 *
 * Expresses the shape common to periodic background jobs (Home Assistant REST
 * light-sensor polling, optional telemetry pollers): wait for WiFi once, then
 * loop, suspending while an OTA transfer is in progress or (optionally) while a
 * gate flag is clear, call a poll function, and sleep for a live-config interval
 * with optional exponential backoff on failure.
 *
 * The pure backoff step lives in network_poll_backoff.h (host-testable); this
 * translation unit owns the FreeRTOS wait/notify plumbing around it. The loop
 * carries no hard dependency on any other subsystem: the OTA stand-down gate and
 * the activity gate are passed in as optional flag pointers.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;              /**< Log tag for this poll loop. */
    EventGroupHandle_t wifi_group; /**< Waited on once before the first poll; NULL skips the wait. */
    EventBits_t wifi_bits;         /**< Bits to wait for (e.g. a WIFI_CONNECTED bit). */

    /** Optional OTA stand-down gate; NULL disables. While *ota_gate is true the
     * loop suspends (no polling), matching system.md 10.3. */
    const volatile bool *ota_gate;

    /** Optional activity gate; NULL = always active. While the pointed-to flag is
     * false the loop suspends. */
    _Atomic bool *active_gate;

    /** Perform one poll. Return false on failure (triggers backoff if configured).
     * @p arg is the opaque pointer passed to network_poll_loop_run(). */
    bool (*poll_once)(void *arg);

    /** Live-config poll interval in ms, re-read every cycle so a config change
     * takes effect on the next sleep. */
    uint32_t (*interval_ms)(void *arg);

    uint32_t backoff_initial_ms; /**< 0 = no failure backoff (retry at interval_ms()). */
    uint32_t backoff_max_ms;     /**< Ceiling the doubling saturates at; 0 = unbounded. */
} network_poll_spec_t;

/**
 * Run the poll loop described by @p spec. Never returns; call this as (or from) a
 * FreeRTOS task entry point. A task notification (xTaskNotifyGive) delivered to
 * the running task wakes it early from an interval sleep or a gate suspension.
 */
void network_poll_loop_run(const network_poll_spec_t *spec, void *arg);

#ifdef __cplusplus
}
#endif
