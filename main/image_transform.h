#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "app_config.h"      /* image_transform_t */
#include "image_decode.h"    /* image_frame_meta_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transform stage: composes the active decoded frame into a panel-sized RGB565
 * present buffer, applying per-image scale, 90-degree rotation, pan offset and a
 * global color-temperature adjustment. The whole panel is written every call
 * (covered area sampled, remainder black-filled), so the LVGL sink receives a
 * complete opaque frame and no edge-erase is needed.
 *
 * A reuse cache holds the last PPA/software scaled buffer keyed on the active
 * frame generation plus scaleX/scaleY, so offset-only or rotation-only or
 * color-temperature-only changes recompose without rescaling.
 */

/**
 * @brief One-time init. @p scaled_buf is the reuse/scale scratch buffer
 *        (>= panel_w*panel_h*4*2 bytes recommended), 128-byte aligned PSRAM.
 */
esp_err_t image_transform_init(uint16_t *scaled_buf, size_t scaled_cap,
                               int panel_w, int panel_h);

/** Invalidate the scaled reuse cache (call after every active-buffer swap). */
void image_transform_invalidate_cache(void);

/**
 * @brief Render @p active through @p tf + @p color_temp into @p present.
 *
 * @param active      Active decoded RGB565 buffer.
 * @param meta        Geometry of @p active (width/height/stride).
 * @param tf          Per-image transform (scale/offset/rotation).
 * @param color_temp  Global color temperature in Kelvin (6500 = neutral).
 * @param generation  Active-buffer generation counter (for cache validation).
 * @param present     Panel-sized (panel_w*panel_h) RGB565 output buffer.
 * @param wdt_feed    Optional watchdog-feed callback for long software passes.
 */
esp_err_t image_transform_render(const uint16_t *active, const image_frame_meta_t *meta,
                                 const image_transform_t *tf, int color_temp,
                                 uint32_t generation, uint16_t *present,
                                 void (*wdt_feed)(void));

#ifdef __cplusplus
}
#endif
