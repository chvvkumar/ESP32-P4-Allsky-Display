#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Touch gesture recognizer.
 *
 * Runs a whole-screen tap / double-tap state machine over the LVGL input device
 * created by the BSP GT911 driver. A 50 ms lv_timer samples the device and drives
 * the recognizer with the timing tolerances from the input-display spec:
 *   debounce 50 ms, valid tap 50-1000 ms, double-tap window 400 ms.
 *
 * There are no zones, swipe, or long-press gestures. Presses longer than the tap
 * ceiling are silently ignored. While the moon page is active (see
 * input_context.h) the recognizer is held IDLE and emits nothing.
 */

/* Gesture callbacks. Any field may be NULL. Invoked from the LVGL task context. */
typedef struct {
    /* A single tap was recognized: advance to the next image source. The image
     * pipeline decides effectiveness (only acts when cycling is on and more than
     * one source is configured). */
    void (*single_tap)(void);
    /* A double tap was recognized: toggle between auto-cycling and
     * single-image-refresh mode (runtime only, not persisted). */
    void (*double_tap)(void);
} input_gesture_hooks_t;

/* Acquire the BSP input device, register the gesture hooks, and start the 50 ms
 * sampling timer. Call after bsp_display_start(). `hooks` may be NULL (touch is
 * still consumed for the moon page, but no tap actions fire). If the BSP reports
 * no input device, touch is disabled for the session and this returns ESP_OK
 * (boot must never be blocked by touch failure). */
esp_err_t input_touch_init(const input_gesture_hooks_t *hooks);

/* Fix-forward toggle for the single-tap dispatch latency. When false, a valid
 * single tap fires immediately instead of waiting out the double-tap window.
 * Default true (legacy behavior: single tap delayed up to 400 ms). */
void input_touch_set_double_tap_enabled(bool enabled);

#ifdef __cplusplus
}
#endif
