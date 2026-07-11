#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cross-subsystem input context.
 *
 * Shared contract between the input/gesture recognizer and the computed-moon
 * page. The moon page marks itself active while it is the displayed source; the
 * tap/double-tap recognizer reads that flag and, while set, forces itself IDLE
 * and clears pending gesture flags every poll, so no tap can advance the
 * slideshow or toggle cycling and no half-finished tap survives the page change.
 * The moon drag handler owns the touch stream while its page is active and reads
 * touch through input_touch_read().
 */

/* Set by the moon subsystem when entering (true) / leaving (false) its page.
 * Idempotent; cheap. */
void input_set_moon_page_active(bool active);

/* Read by the gesture recognizer. */
bool input_moon_page_active(void);

/* Convenience touch read for the moon drag handler (and any future page that
 * needs raw touch). Returns true if a finger is currently down, writing the
 * panel-space coordinates into *x and *y (either may be NULL). Coordinates are
 * in the active panel resolution with no axis swap or mirroring.
 *
 * Must be called from the LVGL task context (for example inside an lv_timer or
 * while holding bsp_display_lock()), like all LVGL access. Returns false if the
 * input device is not yet initialized. */
bool input_touch_read(int *x, int *y);

#ifdef __cplusplus
}
#endif
