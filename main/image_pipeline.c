/**
 * @file image_pipeline.c
 * @brief Fetch/decode/transform/display core and cycling scheduler.
 */

#include "image_pipeline.h"
#include "image_download.h"
#include "image_decode.h"
#include "image_transform.h"
#include "image_ppa.h"
#include "image_sink.h"
#include "moon_source.h"
#include "input_context.h"
#include "app_config.h"
#include "config_debounce.h"
#include "display_defs.h"

#include <string.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "img_pipe";

#define IMG_MAX_DIM            1448
/* Holds an MCU-aligned decode of content up to IMG_MAX_DIM (1456*1456*2). */
#define IMG_FULL_BUF_BYTES     (1456u * 1456u * 2u)
#define IMG_DOWNLOAD_FLOOR     (3u * 1024u * 1024u)

#define LOOP_PERIOD_MS         50
#define STATE_CACHE_MS         5000     /* re-read cycling state at most every 5 s */
#define TUNE_AUTO_CLEAR_MS     600000   /* 10 min tune-pause safety backstop */
#define FAST_RETRY_MS          15000
#define FAST_RETRY_MAX         3
#define FETCH_STUCK_MS         100000   /* IMAGE_PROCESS_TIMEOUT force-clear */
#define NO_WIFI_LOG_MS         5000
#define WORKER_CORE            0
#define LOOP_CORE              1

/* ---- Buffers ------------------------------------------------------------ */
static uint8_t  *s_download;
static size_t    s_download_cap;
static uint16_t *s_bufA;          /* one of active/pending */
static uint16_t *s_bufB;
static uint16_t *s_scaled;
static uint16_t *s_present[2];
static int       s_present_idx;   /* index of the buffer being composed next */
static int       s_pw, s_ph;

/* ---- Double-buffer swap state (guarded by s_buf_mtx) -------------------- */
static SemaphoreHandle_t s_buf_mtx;
static uint16_t         *s_active;
static uint16_t         *s_pending;
static image_frame_meta_t s_active_meta;
static image_frame_meta_t s_pending_meta;
static int               s_active_idx = -1;   /* source slot of active frame (-1 = legacy) */
static int               s_pending_idx = -1;
static uint32_t          s_generation;
static bool              s_active_valid;

/* ---- Handshake flags ---------------------------------------------------- */
static _Atomic bool s_fetch_trigger;
static _Atomic bool s_fetching;
static _Atomic bool s_frame_ready;
static _Atomic bool s_cycle_failed;
static int64_t      s_fetch_start_us;

/* ---- Command request flags (set by other tasks, consumed by loop) ------- */
static _Atomic bool s_req_refresh;
static _Atomic bool s_req_next;
static _Atomic bool s_req_toggle_single;
static _Atomic bool s_req_rerender;
static _Atomic bool s_tune_pause;
static _Atomic int64_t s_edit_ts_ms;

/* ---- Scheduler state (loop task only) ----------------------------------- */
static bool     s_single_refresh;
static int64_t  s_last_update_ms;
static int64_t  s_last_cycle_ms;
static int64_t  s_last_success_ms;
static int      s_retry_count;
static int64_t  s_last_nowifi_log_ms;
static bool     s_stale_shown;
static bool     s_first_image;

/* cached cycling state */
static int64_t  s_state_cache_ms;
static bool     s_c_cycling, s_c_random;
static int      s_c_mode, s_c_count;
static uint32_t s_c_update_interval;

/* shuffle permutation */
static int  s_perm[ALLSKY_MAX_SOURCES];
static int  s_perm_len;
static int  s_perm_pos;
static int  s_perm_count;

static const image_pipeline_deps_t *s_deps;

/* ---- helpers ------------------------------------------------------------ */

static bool wifi_ok(void)
{
    return (s_deps && s_deps->wifi_connected) ? s_deps->wifi_connected() : true;
}

static bool ota_active(void)
{
    return (s_deps && s_deps->ota_in_progress) ? s_deps->ota_in_progress() : false;
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void wdt_feed(void)
{
    esp_task_wdt_reset();
}

static bool url_is_moon(const char *url)
{
    return strncmp(url, "moon://", 7) == 0;
}

bool image_pipeline_current_is_moon(void)
{
    char url[256];
    app_config_current_image_url(url, sizeof(url));
    return url_is_moon(url);
}

bool image_pipeline_has_first_image(void)
{
    return s_first_image;
}

/* ---- currentImageIndex debounced persistence ---------------------------- */
static config_debounce_t s_idx_debounce;

static void idx_flush_cb(void *arg)
{
    (void)arg;
    app_config_save();
}

/* ---- cycling state cache ------------------------------------------------ */
static void refresh_state_cache(int64_t now)
{
    if (s_state_cache_ms != 0 && now - s_state_cache_ms < STATE_CACHE_MS) {
        return;
    }
    s_state_cache_ms = now;
    s_c_cycling = app_config_get_cycling_en();
    s_c_random = app_config_get_random_ord();
    s_c_mode = (int)app_config_get_img_upd_mode();
    s_c_count = app_config_source_count();
    s_c_update_interval = app_config_get_upd_interval();
    if (s_c_update_interval == 0) {
        s_c_update_interval = 120000;
    }
}

/* ---- shuffle ------------------------------------------------------------ */
static int shuffle_next(int count, int cur)
{
    if (!s_perm_len || s_perm_count != count || s_perm_pos >= s_perm_len) {
        s_perm_len = app_config_build_shuffle_order(s_perm, ALLSKY_MAX_SOURCES);
        s_perm_pos = 0;
        s_perm_count = count;
    }
    /* Scan the permutation for the next enabled index different from cur; rebuild
     * once if exhausted. */
    for (int scanned = 0; scanned < s_perm_len; ++scanned) {
        int idx = s_perm[s_perm_pos++];
        if (s_perm_pos >= s_perm_len) {
            s_perm_len = app_config_build_shuffle_order(s_perm, ALLSKY_MAX_SOURCES);
            s_perm_pos = 0;
            s_perm_count = count;
        }
        if (idx == cur) {
            continue;
        }
        if (idx >= 0 && idx < count && app_config_get_source_enabled(idx)) {
            return idx;
        }
    }
    return cur;
}

/* Advance the current index (2.3). Returns the new index (persisted debounced). */
static void advance_next_image(void)
{
    int count = app_config_source_count();
    if (count <= 1) {
        ESP_LOGI(TAG, "next image: only %d source(s), no-op", count);
        return;
    }
    int cur = (int)app_config_get_curr_img_idx();
    int chosen = cur;

    if (app_config_get_random_ord()) {
        chosen = shuffle_next(count, cur);
    } else {
        int idx = cur;
        for (int i = 0; i < 2 * count; ++i) {
            idx = (idx + 1) % count;
            if (app_config_get_source_enabled(idx)) {
                chosen = idx;
                break;
            }
        }
    }

    if (chosen != cur) {
        app_config_set_curr_img_idx(chosen);
        config_debounce_touch(&s_idx_debounce);
    }
    ESP_LOGI(TAG, "advance image %d -> %d", cur, chosen);
}

/* ---- worker: produce a frame into the pending buffer -------------------- */

static bool produce_moon(uint16_t *dst, image_frame_meta_t *meta)
{
    esp_err_t err = moon_render_to_buffer(dst, s_pw, s_ph);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "moon render failed (%s)", esp_err_to_name(err));
        return false;
    }
    meta->width = (uint16_t)s_pw;
    meta->height = (uint16_t)s_ph;
    meta->stride_px = (uint16_t)s_pw;
    return true;
}

static bool produce_http(const char *url, uint16_t *dst, image_frame_meta_t *meta)
{
    if (!wifi_ok()) {
        ESP_LOGW(TAG, "fetch aborted: Wi-Fi down");
        return false;
    }

    image_dl_req_t req = {
        .url = url,
        .skip_tls_verify = false,
        .buf = s_download,
        .buf_cap = s_download_cap,
        .watchdog_feed = wdt_feed,
    };
    size_t len = 0;
    image_dl_status_t st = image_download(&req, &len);
    if (st != IMG_DL_OK) {
        ESP_LOGW(TAG, "download failed (%s) for %s", image_dl_status_str(st), url);
        return false;
    }

    esp_err_t err = image_decode(s_download, len, dst, IMG_FULL_BUF_BYTES,
                                 IMG_MAX_DIM, wdt_feed, meta);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "decode failed (%s) for %s", esp_err_to_name(err), url);
        return false;
    }
    return true;
}

static void worker_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();
        if (!atomic_load(&s_fetch_trigger)) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        atomic_store(&s_fetch_trigger, false);
        atomic_store(&s_fetching, true);
        s_fetch_start_us = esp_timer_get_time();

        /* Resolve the source and its slot under the config lock's accessors. */
        char url[256];
        app_config_current_image_url(url, sizeof(url));
        int idx = -1;
        if (app_config_get_cycling_en() && app_config_source_count() > 0) {
            idx = (int)app_config_get_curr_img_idx();
        }

        bool ok;
        if (url_is_moon(url)) {
            ok = produce_moon(s_pending, &s_pending_meta);
        } else {
            ok = produce_http(url, s_pending, &s_pending_meta);
        }

        if (ok) {
            s_pending_idx = idx;
            atomic_store(&s_cycle_failed, false);
            atomic_store(&s_frame_ready, true);
        } else {
            atomic_store(&s_cycle_failed, true);
        }
        atomic_store(&s_fetching, false);
    }
}

/* ---- loop: swap, render, schedule --------------------------------------- */

/* Build the effective transform for the active frame's source slot. */
static void active_transform(image_transform_t *tf, bool *is_moon)
{
    int idx = s_active_idx;
    char url[256];
    if (idx >= 0) {
        app_config_get_source_url(idx, url, sizeof(url));
    } else {
        app_config_get_image_url(url, sizeof(url));
    }
    *is_moon = url_is_moon(url);

    image_source_t src;
    if (idx >= 0 && app_config_get_source(idx, &src)) {
        *tf = src.transform;
    } else {
        tf->scale_x = app_config_get_def_scale_x();
        tf->scale_y = app_config_get_def_scale_y();
        tf->offset_x = app_config_get_def_off_x();
        tf->offset_y = app_config_get_def_off_y();
        tf->rotation = app_config_get_def_rot();
    }

    if (*is_moon) {
        /* Moon render is already disk-scaled; force identity scale/rotation, keep
         * pan offsets. */
        tf->scale_x = 1.0f;
        tf->scale_y = 1.0f;
        tf->rotation = 0.0f;
    }
}

static void render_active(void)
{
    if (!s_active_valid) {
        return;
    }
    image_transform_t tf;
    bool is_moon;
    active_transform(&tf, &is_moon);

    uint16_t *present = s_present[s_present_idx];
    image_transform_render(s_active, &s_active_meta, &tf, (int)app_config_get_color_temp(),
                           s_generation, present, wdt_feed);

    if (bsp_display_lock(1000)) {
        image_sink_present(present, s_pw, s_ph);
        bsp_display_unlock();
        s_present_idx ^= 1; /* next compose targets the other buffer */
    } else {
        ESP_LOGW(TAG, "display lock timeout on present");
    }
}

/* Swap pending -> active on frame-ready. Returns true if a swap happened. */
static bool try_swap(void)
{
    if (!atomic_load(&s_frame_ready)) {
        return false;
    }
    if (ota_active()) {
        return false;
    }
    if (xSemaphoreTake(s_buf_mtx, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    uint16_t *tmp = s_active;
    s_active = s_pending;
    s_pending = tmp;
    s_active_meta = s_pending_meta;
    s_active_idx = s_pending_idx;
    s_active_valid = true;
    s_generation++;
    atomic_store(&s_frame_ready, false);
    xSemaphoreGive(s_buf_mtx);

    image_transform_invalidate_cache();

    s_last_success_ms = now_ms();
    s_retry_count = 0;
    s_first_image = true;

    /* The displayed source just changed: tell the input layer whether the moon
     * page is now active so touch drives drag-to-rotate instead of slideshow
     * navigation. */
    input_set_moon_page_active(image_pipeline_current_is_moon());
    return true;
}

/* Schedule a download by raising the worker trigger (6.2/6.3). */
static void schedule_download(bool normal, int64_t now)
{
    if (!wifi_ok()) {
        if (now - s_last_nowifi_log_ms >= NO_WIFI_LOG_MS) {
            ESP_LOGW(TAG, "skipping download: no Wi-Fi");
            s_last_nowifi_log_ms = now;
        }
        return; /* do NOT touch last_update: fire immediately once Wi-Fi returns */
    }
    atomic_store(&s_fetch_trigger, true);
    s_last_update_ms = now;
    if (normal) {
        s_retry_count = 0;
    } else {
        s_retry_count++;
    }
}

static bool pipeline_busy(void)
{
    return atomic_load(&s_fetching) || atomic_load(&s_fetch_trigger) ||
           atomic_load(&s_frame_ready);
}

static void process_commands(int64_t now)
{
    if (atomic_exchange(&s_req_toggle_single, false)) {
        s_single_refresh = !s_single_refresh;
        ESP_LOGI(TAG, "single-image-refresh %s", s_single_refresh ? "on" : "off");
    }
    if (atomic_exchange(&s_req_next, false)) {
        advance_next_image();
        s_last_cycle_ms = now;
        s_last_update_ms = 0; /* force immediate download */
    }
    if (atomic_exchange(&s_req_refresh, false)) {
        s_last_update_ms = 0; /* force immediate download */
    }
    if (atomic_exchange(&s_req_rerender, false)) {
        render_active(); /* re-apply transform/color temp without re-download */
    }
}

static void loop_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);

    for (;;) {
        esp_task_wdt_reset();
        int64_t now = now_ms();

        if (ota_active()) {
            vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
            continue;
        }

        process_commands(now);
        refresh_state_cache(now);

        /* Force-clear a stuck fetch guard (3.1). */
        if (atomic_load(&s_fetching) &&
            (esp_timer_get_time() - s_fetch_start_us) > (int64_t)FETCH_STUCK_MS * 1000) {
            ESP_LOGW(TAG, "fetch stuck > %d ms, clearing guard", FETCH_STUCK_MS);
            atomic_store(&s_fetching, false);
        }

        /* Tune-pause auto-clear (2.7). */
        if (atomic_load(&s_tune_pause)) {
            int64_t edit = atomic_load(&s_edit_ts_ms);
            if (edit != 0 && now - edit >= TUNE_AUTO_CLEAR_MS) {
                atomic_store(&s_tune_pause, false);
                ESP_LOGI(TAG, "tune pause auto-cleared");
            }
        }

        /* Consume a ready frame. */
        if (try_swap()) {
            render_active();
        }

        bool busy = pipeline_busy();
        bool tune_paused = atomic_load(&s_tune_pause);

        /* Cycle timer (2.4). */
        bool cycle_fired = false;
        if (s_c_mode == 0 && s_c_cycling && s_c_count > 1 && !busy &&
            !s_single_refresh && !tune_paused) {
            int cur = (int)app_config_get_curr_img_idx();
            uint32_t dur_s = app_config_get_source_duration(cur);
            int64_t dur_ms = (int64_t)dur_s * 1000;
            if (s_last_cycle_ms == 0 || now - s_last_cycle_ms >= dur_ms) {
                advance_next_image();
                s_last_cycle_ms = now;
                cycle_fired = true;
            }
        }

        /* Refresh scheduling (6.2). */
        if (!busy && !pipeline_busy()) {
            bool want = false, normal = true;
            if (s_c_mode == 0) {
                if (cycle_fired || s_last_update_ms == 0 ||
                    now - s_last_update_ms >= (int64_t)s_c_update_interval) {
                    want = true;
                    normal = true;
                }
            } else { /* API-triggered */
                if (s_last_update_ms == 0) {
                    want = true;
                    normal = true;
                }
            }
            if (!want && atomic_load(&s_cycle_failed) && s_retry_count < FAST_RETRY_MAX &&
                s_last_update_ms != 0 && now - s_last_update_ms >= FAST_RETRY_MS) {
                want = true;
                normal = false;
            }
            if (want) {
                schedule_download(normal, now);
            }
        }

        /* Stale-image badge after 3x the refresh interval (locked robustness). */
        if (s_first_image) {
            bool stale = (now - s_last_success_ms) > (int64_t)s_c_update_interval * 3;
            if (stale != s_stale_shown) {
                s_stale_shown = stale;
                if (bsp_display_lock(200)) {
                    image_sink_set_stale(stale);
                    bsp_display_unlock();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_PERIOD_MS));
    }
}

/* ---- command surface ---------------------------------------------------- */

void image_pipeline_request_refresh(void)
{
    atomic_store(&s_req_refresh, true);
}

void image_pipeline_next_image(void)
{
    atomic_store(&s_req_next, true);
}

void image_pipeline_toggle_single_refresh(void)
{
    atomic_store(&s_req_toggle_single, true);
}

void image_pipeline_set_tune_pause(bool paused)
{
    atomic_store(&s_tune_pause, paused);
    atomic_store(&s_edit_ts_ms, now_ms());
    ESP_LOGI(TAG, "tune pause %s", paused ? "set" : "cleared");
}

void image_pipeline_notify_edit(void)
{
    atomic_store(&s_edit_ts_ms, now_ms());
    atomic_store(&s_req_rerender, true);
}

/* ---- init / start ------------------------------------------------------- */

static uint16_t *alloc_psram(size_t bytes, const char *what)
{
    uint16_t *p = heap_caps_aligned_alloc(128, bytes, MALLOC_CAP_SPIRAM);
    if (!p) {
        ESP_LOGE(TAG, "PSRAM alloc failed: %s (%u bytes, free %u)", what, (unsigned)bytes,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
    return p;
}

esp_err_t image_pipeline_init(const image_pipeline_deps_t *deps)
{
    s_deps = deps;

    allsky_panel_profile_t panel = allsky_panel_profile();
    s_pw = panel.width;
    s_ph = panel.height;

    size_t panel_bytes = (size_t)s_pw * s_ph * 2;
    s_download_cap = panel_bytes > IMG_DOWNLOAD_FLOOR ? panel_bytes : IMG_DOWNLOAD_FLOOR;
    size_t scaled_cap = panel_bytes * 4; /* 4x panel pixels for scale headroom */

    s_download = heap_caps_aligned_alloc(128, s_download_cap, MALLOC_CAP_SPIRAM);
    s_bufA = alloc_psram(IMG_FULL_BUF_BYTES, "active");
    s_bufB = alloc_psram(IMG_FULL_BUF_BYTES, "pending");
    s_scaled = alloc_psram(scaled_cap, "scaled");
    s_present[0] = alloc_psram(panel_bytes, "presentA");
    s_present[1] = alloc_psram(panel_bytes, "presentB");

    if (!s_download || !s_bufA || !s_bufB || !s_scaled || !s_present[0] || !s_present[1]) {
        ESP_LOGE(TAG, "fatal: image buffer allocation failed");
        return ESP_ERR_NO_MEM;
    }

    s_active = s_bufA;
    s_pending = s_bufB;
    s_present_idx = 0;

    s_buf_mtx = xSemaphoreCreateMutex();
    if (!s_buf_mtx) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = image_ppa_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PPA init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = image_transform_init(s_scaled, scaled_cap, s_pw, s_ph);
    if (err != ESP_OK) {
        return err;
    }

    err = config_debounce_init(&s_idx_debounce, 5000, idx_flush_cb, NULL);
    if (err != ESP_OK) {
        return err;
    }

    if (bsp_display_lock(1000)) {
        err = image_sink_create();
        bsp_display_unlock();
    } else {
        err = ESP_ERR_TIMEOUT;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sink create failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "init: panel %dx%d, download %u KB, full %u KB x2, scaled %u KB",
             s_pw, s_ph, (unsigned)(s_download_cap / 1024),
             (unsigned)(IMG_FULL_BUF_BYTES / 1024), (unsigned)(scaled_cap / 1024));
    return ESP_OK;
}

esp_err_t image_pipeline_start(void)
{
    BaseType_t r1 = xTaskCreatePinnedToCore(worker_task, "img_worker", 16 * 1024, NULL,
                                            2, NULL, WORKER_CORE);
    BaseType_t r2 = xTaskCreatePinnedToCore(loop_task, "img_loop", 8 * 1024, NULL,
                                            3, NULL, LOOP_CORE);
    if (r1 != pdPASS || r2 != pdPASS) {
        ESP_LOGE(TAG, "task creation failed");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "pipeline started");
    return ESP_OK;
}
