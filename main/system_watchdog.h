#pragma once

/*
 * Task watchdog management and watchdog-safe timing helpers.
 *
 * The task watchdog supervises the main task and the image download task. On a
 * supervised task starving past the configured timeout the action is
 * panic-and-reboot: the panic path is captured by the crash logger (backtrace
 * plus reset-reason classification) so a starvation reboot is preserved as a
 * crash for forensics.
 *
 * Feeding: force_feed resets the calling task's watchdog unconditionally; feed
 * is the same operation exposed under the name the main loop uses between steps.
 * Long operations use the watchdog-safe delay/yield helpers, which sleep in
 * bounded slices and feed on every pass so a legitimately slow operation never
 * trips the watchdog.
 */

#include "esp_err.h"
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deinitialize any watchdog configured by the bootloader/startup, then initialize
 * (or reconfigure) the task watchdog with `timeout_ms`, idle tasks unmonitored,
 * trigger action panic-and-reboot. Subscribes the calling (main) task. Idempotent:
 * a second call reconfigures the timeout. Returns ESP_OK on success. */
esp_err_t system_watchdog_init(uint32_t timeout_ms);

/* Subscribe the calling task to the watchdog. "Already subscribed" is success.
 * Called by the image download task at its own start. */
esp_err_t system_watchdog_subscribe_current(void);

/* Unsubscribe the calling task (used by tasks that self-delete). */
esp_err_t system_watchdog_unsubscribe_current(void);

/* Reset (feed) the calling task's watchdog. Both entry points feed unconditionally;
 * the two names exist so call sites read intent. Safe to call from any subscribed
 * task; a call from an unsubscribed task is a no-op. */
void system_watchdog_feed(void);
void system_watchdog_force_feed(void);

/* Watchdog-safe delay: sleeps for `ms` in slices of at most 100 ms, feeding the
 * calling task's watchdog before each slice. Use instead of vTaskDelay for any
 * delay longer than a couple of ticks inside a supervised task. */
void system_watchdog_delay_ms(uint32_t ms);

/* Feed, yield to same-priority tasks, then sleep one tick. */
void system_watchdog_yield(void);

#ifdef __cplusplus
}
#endif
