#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Integration surface for the computed-Moon image source.
 *
 * The image pipeline treats any source whose URL begins with "moon://" as a
 * locally rendered frame: instead of an HTTP download it calls
 * moon_render_to_buffer() to fill the pending image buffer at native panel
 * resolution, then swaps and displays it exactly like a decoded JPEG.
 *
 * A separate render task drives the interactive drag-to-rotate view so the
 * gesture never blocks the pipeline's scheduling loop.
 */

#define MOON_URL_PREFIX   "moon://"

/* True if `url` designates the computed Moon source. */
bool moon_source_is_moon_url(const char *url);

/* Lazily prepare the lunar texture (idempotent, mutex-guarded). Runs the
 * one-time disk-scale migration for stored moon sources on first success.
 * Returns ESP_OK once the texture (embedded or procedural) is ready. */
esp_err_t moon_source_init(void);

/* Render a resting Moon frame into caller buffer `dst` (w*h RGB565, native panel
 * resolution). `disk_scale` is the fraction of the frame the disc fills
 * (per-source scaleX; clamped internally to [0.2, 4.0]).
 *
 * Returns:
 *   ESP_OK                  frame written into dst
 *   ESP_ERR_INVALID_STATE   wall-clock time not yet synced (epoch < 2020-01-01);
 *                           the pipeline should treat this as a failed update
 *   ESP_ERR_INVALID_ARG     bad arguments
 *   ESP_FAIL                texture unavailable or render failure
 */
esp_err_t moon_render_to_buffer_scaled(uint16_t *dst, int w, int h, float disk_scale);

/* Convenience form: the disk scale is taken from the current image source's
 * stored scaleX (default 0.8). Use this from the moon:// branch of the pipeline. */
esp_err_t moon_render_to_buffer(uint16_t *dst, int w, int h);

/* ---- Interactive drag-to-rotate driver --------------------------------- */

/* Blit sink: the display/pipeline layer registers a function that upscales the
 * freshly rendered `src` frame (src_w x src_h RGB565, tightly packed) to the
 * panel and presents it. The pointer is valid only for the duration of the
 * call. */
typedef void (*moon_blit_cb_t)(const uint16_t *src, int src_w, int src_h);

/* Settle sink: invoked once when a gesture fully settles home, so the pipeline
 * can force a crisp full-resolution resting re-render that replaces the last
 * upscaled interactive frame. May be NULL. */
typedef void (*moon_settle_cb_t)(void);

/* Create the interactive render task (idempotent) and register the sinks. Call
 * once at boot after the display is up. The task idles on a notification and
 * only produces frames while a gesture is live. */
esp_err_t moon_interactive_init(moon_blit_cb_t blit, moon_settle_cb_t on_settle);

/* Wake the interactive render task. Call from the touch layer the instant a
 * press is promoted to a rotate (after moon_drag_begin + the first
 * moon_drag_move that crosses the promotion threshold). The task self-idles once
 * the disc settles home and no free-spin hold remains. Safe to call repeatedly. */
void moon_interactive_wake(void);

/* True while the interactive render task is producing frames. The input layer
 * keeps suppressing tap navigation until this returns false. */
bool moon_interactive_running(void);

/* ---- Moonrise / moonset (for MQTT sensors) ------------------------------ */

/* Next/current moonrise and moonset in UTC epoch seconds, using the observer
 * latitude/longitude from configuration. Returns false (and leaves *out_utc
 * untouched) if the location is unset or no event falls in the search window. */
bool moon_source_next_rise(time_t *out_utc);
bool moon_source_next_set(time_t *out_utc);

#ifdef __cplusplus
}
#endif
