/**
 * @file moon_source.c
 * @brief Integration layer for the computed-Moon image source: resting render
 *        into the pipeline's pending buffer, a non-blocking interactive
 *        drag-to-rotate render task, and moonrise/moonset helpers.
 */

#include "moon_source.h"
#include "moon_ephemeris.h"
#include "moon_sphere.h"
#include "moon_interaction.h"
#include "app_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include <string.h>

static const char *TAG = "moon_source";

/* Wall-clock is "synced" once past 2020-01-01 00:00:00 UTC. */
#define MOON_TIME_SYNC_EPOCH   1577836800LL

/* Resting-render tessellation (smooth limb at panel resolution). */
#define MOON_REST_SECTORS      96
#define MOON_REST_STACKS       48

/* Interactive render: one fixed small size upscaled to the panel each frame. */
#define MOON_DRAG_SIZE         240
#define MOON_DRAG_GUARD_ROWS   8      /* zeroed rows below the frame: absorb an upscaler bottom over-read */
#define MOON_DRAG_EASE_ALPHA   0.35f
#define MOON_DRAG_FRAME_MS     15
#define MOON_DRAG_TASK_STACK   8192
#define MOON_DRAG_TASK_PRIO    4
#define MOON_DRAG_TASK_CORE    1

/* ---- Resting render ----------------------------------------------------- */

/* Persistent depth buffer for resting renders, sized to the active panel. */
static uint16_t *s_rest_zbuf     = NULL;
static int       s_rest_zbuf_pix = 0;

static bool ensure_rest_zbuf(int w, int h)
{
    int pix = w * h;
    if (s_rest_zbuf && s_rest_zbuf_pix >= pix) return true;
    if (s_rest_zbuf) {
        heap_caps_free(s_rest_zbuf);
        s_rest_zbuf = NULL;
        s_rest_zbuf_pix = 0;
    }
    s_rest_zbuf = (uint16_t *)heap_caps_aligned_alloc(
        128, (size_t)pix * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!s_rest_zbuf) {
        ESP_LOGE(TAG, "resting z-buffer alloc failed (%d px)", pix);
        return false;
    }
    s_rest_zbuf_pix = pix;
    return true;
}

/* One-time disk-scale normalization for stored moon:// sources. An older config
 * scheme stored a scaleX with a different meaning; bring any out-of-range value
 * back to the disk-scale default so imported/legacy sources render sanely. */
static void migrate_moon_scales(void)
{
    if (app_config_get_mScaleMig()) return;

    int n = app_config_source_count();
    for (int i = 0; i < n; i++) {
        image_source_t src;
        if (!app_config_get_source(i, &src)) continue;
        if (!moon_source_is_moon_url(src.url)) continue;
        if (src.transform.scale_x < 0.2f || src.transform.scale_x > 4.0f) {
            src.transform.scale_x = 0.8f;
            app_config_set_source_transform(i, &src.transform);
        }
    }
    app_config_set_mScaleMig(true);
    app_config_save();
}

bool moon_source_is_moon_url(const char *url)
{
    return url && strncmp(url, MOON_URL_PREFIX, sizeof(MOON_URL_PREFIX) - 1) == 0;
}

esp_err_t moon_source_init(void)
{
    if (!moon_sphere_init()) {
        return ESP_FAIL;
    }
    migrate_moon_scales();
    return ESP_OK;
}

esp_err_t moon_render_to_buffer_scaled(uint16_t *dst, int w, int h, float disk_scale)
{
    if (!dst || w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;

    time_t now = time(NULL);
    if ((long long)now < MOON_TIME_SYNC_EPOCH) {
        return ESP_ERR_INVALID_STATE;   /* clock not synced: skip this cycle */
    }

    if (moon_source_init() != ESP_OK) return ESP_FAIL;
    if (!ensure_rest_zbuf(w, h))       return ESP_FAIL;

    moon_state_t st;
    moon_compute(now, (double)app_config_get_moonLat(),
                 (double)app_config_get_moonLon(), &st);

    uint8_t bg = (uint8_t)(app_config_get_moonBg() & 0x3);

    uint16_t *r = moon_sphere_render_into(
        w, h, &st, MOON_REST_SECTORS, MOON_REST_STACKS, bg,
        0.0f, 0.0f, MOON_LIGHT_TRUE_PHASE, disk_scale, dst, s_rest_zbuf);

    return r ? ESP_OK : ESP_FAIL;
}

esp_err_t moon_render_to_buffer(uint16_t *dst, int w, int h)
{
    float disk_scale = 0.8f;
    int idx = (int)app_config_get_curr_img_idx();
    image_source_t src;
    if (app_config_get_source(idx, &src) && moon_source_is_moon_url(src.url)) {
        disk_scale = src.transform.scale_x;
    }
    return moon_render_to_buffer_scaled(dst, w, h, disk_scale);
}

/* ---- Interactive drag-to-rotate driver --------------------------------- */

static TaskHandle_t      s_drag_task   = NULL;
static moon_blit_cb_t    s_blit_cb     = NULL;
static moon_settle_cb_t  s_settle_cb   = NULL;
static volatile bool     s_running     = false;

static uint16_t *s_drag_color = NULL;   /* MOON_DRAG_SIZE x (MOON_DRAG_SIZE+guard) */
static uint16_t *s_drag_zbuf  = NULL;   /* MOON_DRAG_SIZE x MOON_DRAG_SIZE          */

static bool drag_buffers_alloc(void)
{
    if (s_drag_color && s_drag_zbuf) return true;

    size_t color_pix = (size_t)MOON_DRAG_SIZE * (MOON_DRAG_SIZE + MOON_DRAG_GUARD_ROWS);
    size_t z_pix     = (size_t)MOON_DRAG_SIZE * MOON_DRAG_SIZE;

    if (!s_drag_color) {
        s_drag_color = (uint16_t *)heap_caps_aligned_alloc(
            128, color_pix * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    }
    if (!s_drag_zbuf) {
        s_drag_zbuf = (uint16_t *)heap_caps_aligned_alloc(
            128, z_pix * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    }
    if (!s_drag_color || !s_drag_zbuf) {
        ESP_LOGE(TAG, "interactive drag buffer alloc failed");
        return false;
    }
    /* Zero the guard rows once; per-frame renders only touch the top SIZE rows. */
    memset(s_drag_color, 0, color_pix * sizeof(uint16_t));
    return true;
}

static bool interactive_should_run(void)
{
    return moon_drag_active() || !moon_drag_settled() || moon_drag_freespin_pending();
}

static void interactive_session(void)
{
    if (!interactive_should_run()) return;
    if (!drag_buffers_alloc())     return;

    /* Snapshot the ephemeris and per-frame-invariant config once at entry: time
     * does not advance mid-gesture, and disk scale / background are fixed for the
     * whole interaction. */
    time_t now = time(NULL);
    moon_state_t st;
    moon_compute(now, (double)app_config_get_moonLat(),
                 (double)app_config_get_moonLon(), &st);

    uint8_t bg    = (uint8_t)(app_config_get_moonBg() & 0x3);
    uint8_t drag_lm = app_config_get_mDragLM();

    float disk_scale = 0.8f;
    int idx = (int)app_config_get_curr_img_idx();
    image_source_t src;
    if (app_config_get_source(idx, &src) && moon_source_is_moon_url(src.url)) {
        disk_scale = src.transform.scale_x;
    }

    s_running = true;

    while (interactive_should_run()) {
        bool active = moon_drag_active();

        /* Free-spin: once the hold window elapses with the finger up, start the
         * eased return home. */
        if (!active && moon_drag_freespin_pending() &&
            moon_drag_freespin_elapsed(app_config_get_mSpinRet())) {
            moon_drag_trigger_return();
        }

        moon_drag_advance(MOON_DRAG_EASE_ALPHA);

        float yaw, pitch;
        moon_drag_get(&yaw, &pitch);

        /* EXPLORE lighting only while the finger is down; the settle/free-spin
         * frames dissolve back to the true-phase terminator. */
        moon_light_mode_t light =
            (active && drag_lm == 1) ? MOON_LIGHT_EXPLORE : MOON_LIGHT_TRUE_PHASE;

        uint16_t *frame = moon_sphere_render_into(
            MOON_DRAG_SIZE, MOON_DRAG_SIZE, &st,
            MOON_REST_SECTORS, MOON_REST_STACKS, bg,
            yaw, pitch, light, disk_scale, s_drag_color, s_drag_zbuf);

        if (frame && s_blit_cb) {
            s_blit_cb(frame, MOON_DRAG_SIZE, MOON_DRAG_SIZE);
        }

        vTaskDelay(pdMS_TO_TICKS(MOON_DRAG_FRAME_MS));
    }

    s_running = false;

    /* Ask the pipeline for a crisp full-resolution resting re-render to replace
     * the last upscaled interactive frame. */
    if (s_settle_cb) s_settle_cb();
}

static void moon_interactive_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        interactive_session();
    }
}

esp_err_t moon_interactive_init(moon_blit_cb_t blit, moon_settle_cb_t on_settle)
{
    s_blit_cb   = blit;
    s_settle_cb = on_settle;
    if (s_drag_task) return ESP_OK;

    BaseType_t ok = xTaskCreatePinnedToCore(
        moon_interactive_task, "moon_drag", MOON_DRAG_TASK_STACK, NULL,
        MOON_DRAG_TASK_PRIO, &s_drag_task, MOON_DRAG_TASK_CORE);
    if (ok != pdPASS) {
        s_drag_task = NULL;
        ESP_LOGE(TAG, "interactive render task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void moon_interactive_wake(void)
{
    if (s_drag_task) xTaskNotifyGive(s_drag_task);
}

bool moon_interactive_running(void)
{
    return s_running;
}

/* ---- Moonrise / moonset ------------------------------------------------- */

bool moon_source_next_rise(time_t *out_utc)
{
    time_t rise = 0, set = 0;
    bool rv = false, sv = false;
    moon_rise_set(time(NULL),
                  (double)app_config_get_moonLat(),
                  (double)app_config_get_moonLon(),
                  &rise, &rv, &set, &sv);
    if (rv && out_utc) *out_utc = rise;
    return rv;
}

bool moon_source_next_set(time_t *out_utc)
{
    time_t rise = 0, set = 0;
    bool rv = false, sv = false;
    moon_rise_set(time(NULL),
                  (double)app_config_get_moonLat(),
                  (double)app_config_get_moonLon(),
                  &rise, &rv, &set, &sv);
    if (sv && out_utc) *out_utc = set;
    return sv;
}
