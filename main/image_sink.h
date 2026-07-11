#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LVGL display sink for the image pipeline. Owns a single full-screen image
 * widget fed a panel-sized RGB565 buffer at 1:1 (the transform stage has already
 * composed the final frame). Presents are instant (no crossfade); the caller
 * ping-pongs two present buffers so the buffer handed in is never the one LVGL is
 * still flushing. A stale-image badge can be shown over the frame.
 *
 * All functions must be called with the LVGL/display lock held by the caller
 * (bsp_display_lock), matching the esp_lvgl_port threading contract.
 */

/** Create the image widget + stale badge on the active screen. */
esp_err_t image_sink_create(void);

/**
 * @brief Point the widget at a freshly composed panel-sized buffer.
 * The buffer must remain valid until the next present() call.
 */
void image_sink_present(const uint16_t *buf, int w, int h);

/** Show or hide the stale-image badge. */
void image_sink_set_stale(bool stale);

/** True once at least one frame has been presented. */
bool image_sink_has_image(void);

/** The LVGL image object, for input/moon teams to attach touch handlers. */
lv_obj_t *image_sink_widget(void);

#ifdef __cplusplus
}
#endif
