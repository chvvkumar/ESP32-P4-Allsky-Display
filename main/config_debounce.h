#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic debounced-flush helper. Coalesces a burst of "touch" calls into a
 * single deferred flush, so high-frequency mutators (for example the cycling
 * scheduler writing currentImageIndex on every advance) do not wear flash.
 *
 * Not tied to config: any subsystem needing debounced side effects can reuse it.
 * The flush callback runs in the esp_timer task context; keep it short and do
 * not block. Thread-safe for touch/flush_now from any task.
 */
typedef struct {
    esp_timer_handle_t timer;
    uint32_t           delay_ms;
    void             (*flush)(void *arg);
    void              *arg;
} config_debounce_t;

/* Initialize `d` with a flush callback and coalescing delay. */
esp_err_t config_debounce_init(config_debounce_t *d, uint32_t delay_ms,
                               void (*flush)(void *arg), void *arg);

/* (Re)arm the timer: the flush fires delay_ms after the LAST touch. */
esp_err_t config_debounce_touch(config_debounce_t *d);

/* Cancel any pending timer and run the flush immediately (e.g. before reboot). */
void config_debounce_flush_now(config_debounce_t *d);

/* Cancel any pending flush without running it, and release the timer. */
void config_debounce_deinit(config_debounce_t *d);

#ifdef __cplusplus
}
#endif
