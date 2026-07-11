#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Image pipeline: fetch/decode/transform/display and the cycling scheduler.
 *
 * Two tasks cooperate through a double-buffered pending/active pair:
 *   - the worker task downloads + decodes (or renders moon://) into the pending
 *     buffer and raises a frame-ready flag;
 *   - the loop task runs the cycle/refresh scheduler, swaps pending->active on
 *     frame-ready, composes the transform into a present buffer and hands it to
 *     the LVGL sink.
 *
 * Lifecycle: image_pipeline_init() allocates the PSRAM buffers and creates the
 * LVGL widgets (call after the display is started, with app_config initialized).
 * image_pipeline_start() spawns the tasks. The remaining functions are the
 * command surface used by touch, web and MQTT.
 */

typedef struct {
    /* True while Wi-Fi is connected; NULL treated as always-connected. */
    bool (*wifi_connected)(void);
    /* True while a firmware OTA is in progress; NULL treated as never. */
    bool (*ota_in_progress)(void);
} image_pipeline_deps_t;

/** Allocate buffers, register PPA, create LVGL sink. Call with the display lock
 *  available (takes it internally). @p deps may be NULL. */
esp_err_t image_pipeline_init(const image_pipeline_deps_t *deps);

/** Spawn the worker and loop tasks. */
esp_err_t image_pipeline_start(void);

/* ---- Command surface ---------------------------------------------------- */

/** Force an immediate re-download of the current source (resets the refresh
 *  clock). Use for moon settings changes and web/MQTT force-refresh. */
void image_pipeline_request_refresh(void);

/** Advance to the next image (sequential or shuffled), reset the cycle timer and
 *  force an immediate download. Used by touch tap, web and MQTT "next". No-op
 *  when the source count <= 1. */
void image_pipeline_next_image(void);

/** Toggle single-image-refresh mode (double-tap): suppresses cycling while
 *  keeping the periodic refresh of the current image. */
void image_pipeline_toggle_single_refresh(void);

/** Pause/resume cycling while the web UI tunes a transform. Any transform edit
 *  should also call image_pipeline_notify_edit() to refresh the safety timer. */
void image_pipeline_set_tune_pause(bool paused);

/** Refresh the tune-mode activity timestamp and re-render the current frame with
 *  the latest transform / color temperature (no re-download). */
void image_pipeline_notify_edit(void);

/** True if the currently selected source is a moon:// render (touch gestures are
 *  suppressed on moon frames; the moon drag layer handles them instead). */
bool image_pipeline_current_is_moon(void);

/** True once the first image has been shown (suppresses the boot text overlay). */
bool image_pipeline_has_first_image(void);

#ifdef __cplusplus
}
#endif
