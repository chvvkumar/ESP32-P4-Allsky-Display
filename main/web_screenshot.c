#include "web_internal.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "driver/jpeg_encode.h"
#include "lvgl.h"
/* lv_obj_get_ext_draw_size() lives in a private header in this LVGL version;
 * lv_snapshot.c itself includes it the same way. */
#include "src/core/lv_obj_draw_private.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "web_screenshot";

static jpeg_encoder_handle_t s_encoder = NULL;

void web_screenshot_encoder_init(void)
{
    if (s_encoder) return;
    jpeg_encode_engine_cfg_t cfg = { .intr_priority = 0, .timeout_ms = 5000 };
    esp_err_t err = jpeg_new_encoder_engine(&cfg, &s_encoder);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "jpeg_new_encoder_engine failed: %s", esp_err_to_name(err));
        s_encoder = NULL;
    }
}

esp_err_t web_screenshot_handler(httpd_req_t *req)
{
    if (!s_encoder) {
        web_screenshot_encoder_init();
        if (!s_encoder) return web_send_status(req, 500, "error", "JPEG encoder not available", NULL);
    }

    web_hook_display_pause(true);

    if (!bsp_display_lock(5000)) {
        web_hook_display_pause(false);
        ESP_LOGW(TAG, "screenshot: display lock timeout (5000 ms)");
        return web_send_status(req, 503, "error", "Display lock timeout", NULL);
    }

    lv_obj_t *scr = lv_screen_active();
    if (!scr) {
        bsp_display_unlock();
        web_hook_display_pause(false);
        ESP_LOGE(TAG, "screenshot: no active screen");
        return web_send_status(req, 503, "error", "Screenshot capture failed", NULL);
    }

    /* The active screen snapshot needs a w*h*2 RGB565 buffer (~1.3 MB at
     * 800x800). lv_snapshot_take() would allocate that from LVGL's builtin heap
     * (CONFIG_LV_MEM_SIZE_KILOBYTES=64), which always fails. Provide our own
     * PSRAM-backed draw buffer and take the snapshot into it. */
    lv_obj_update_layout(scr);
    int32_t ext = lv_obj_get_ext_draw_size(scr);
    uint32_t w = (uint32_t)(lv_obj_get_width(scr) + ext * 2);
    uint32_t h = (uint32_t)(lv_obj_get_height(scr) + ext * 2);
    uint32_t stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565);
    size_t snap_sz = (size_t)stride * h;

    uint8_t *snap_pix = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, snap_sz, MALLOC_CAP_SPIRAM);
    if (!snap_pix) {
        bsp_display_unlock();
        web_hook_display_pause(false);
        ESP_LOGE(TAG, "screenshot: PSRAM alloc failed (%u bytes)", (unsigned)snap_sz);
        return web_send_status(req, 500, "error", "Out of memory for screenshot", NULL);
    }

    lv_draw_buf_t snap;
    lv_result_t sres = lv_draw_buf_init(&snap, w, h, LV_COLOR_FORMAT_RGB565, stride, snap_pix, snap_sz);
    if (sres == LV_RESULT_OK) {
        sres = lv_snapshot_take_to_draw_buf(scr, LV_COLOR_FORMAT_RGB565, &snap);
    }
    bsp_display_unlock();

    if (sres != LV_RESULT_OK) {
        free(snap_pix);
        web_hook_display_pause(false);
        ESP_LOGE(TAG, "screenshot: capture failed (%ux%u, res=%d)", (unsigned)w, (unsigned)h, (int)sres);
        return web_send_status(req, 503, "error", "Screenshot capture failed", NULL);
    }

    w = snap.header.w;
    h = snap.header.h;
    stride = snap.header.stride;
    uint32_t row = w * 2, raw = row * h;

    jpeg_encode_memory_alloc_cfg_t in_cfg = { .buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER };
    size_t in_sz = 0;
    uint8_t *in = jpeg_alloc_encoder_mem(raw, &in_cfg, &in_sz);
    if (!in) {
        free(snap_pix);
        web_hook_display_pause(false);
        ESP_LOGE(TAG, "screenshot: jpeg input alloc failed (%u bytes)", (unsigned)raw);
        return web_send_status(req, 500, "error", "Out of memory for screenshot", NULL);
    }
    for (uint32_t y = 0; y < h; y++) memcpy(in + y * row, snap.data + y * stride, row);
    free(snap_pix);

    size_t out_cap = raw / 2;
    if (out_cap < 64 * 1024) out_cap = 64 * 1024;
    jpeg_encode_memory_alloc_cfg_t out_cfg = { .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER };
    size_t out_sz = 0;
    uint8_t *out = jpeg_alloc_encoder_mem(out_cap, &out_cfg, &out_sz);
    if (!out) {
        free(in);
        web_hook_display_pause(false);
        ESP_LOGE(TAG, "screenshot: jpeg output alloc failed (%u bytes)", (unsigned)out_cap);
        return web_send_status(req, 500, "error", "Out of memory for screenshot", NULL);
    }

    jpeg_encode_cfg_t enc = {
        .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV444,
        .image_quality = 80,
        .width = w,
        .height = h,
    };
    uint32_t jpg_size = 0;
    esp_err_t err = jpeg_encoder_process(s_encoder, &enc, in, raw, out, out_sz, &jpg_size);
    free(in);
    web_hook_display_pause(false);

    if (err != ESP_OK || jpg_size == 0) {
        free(out);
        ESP_LOGE(TAG, "screenshot: jpeg encode failed (%s, jpg_size=%u)", esp_err_to_name(err), (unsigned)jpg_size);
        return web_send_status(req, 500, "error", "JPEG encode failed", NULL);
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=\"screenshot.jpg\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_send(req, (const char *)out, jpg_size);
    free(out);
    return r;
}
