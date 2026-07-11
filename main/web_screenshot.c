#include "web_internal.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "driver/jpeg_encode.h"
#include "lvgl.h"
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
        return web_send_status(req, 503, "error", "Framebuffer not available", NULL);
    }
    lv_obj_t *scr = lv_screen_active();
    lv_draw_buf_t *snap = scr ? lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB565) : NULL;
    bsp_display_unlock();

    if (!snap || !snap->data) {
        if (snap) lv_draw_buf_destroy(snap);
        web_hook_display_pause(false);
        return web_send_status(req, 503, "error", "Framebuffer not available", NULL);
    }

    uint32_t w = snap->header.w, h = snap->header.h, stride = snap->header.stride;
    uint32_t row = w * 2, raw = row * h;

    jpeg_encode_memory_alloc_cfg_t in_cfg = { .buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER };
    size_t in_sz = 0;
    uint8_t *in = jpeg_alloc_encoder_mem(raw, &in_cfg, &in_sz);
    if (!in) {
        lv_draw_buf_destroy(snap);
        web_hook_display_pause(false);
        return web_send_status(req, 500, "error", "Out of memory for screenshot", NULL);
    }
    for (uint32_t y = 0; y < h; y++) memcpy(in + y * row, snap->data + y * stride, row);
    lv_draw_buf_destroy(snap);

    size_t out_cap = raw / 2;
    if (out_cap < 64 * 1024) out_cap = 64 * 1024;
    jpeg_encode_memory_alloc_cfg_t out_cfg = { .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER };
    size_t out_sz = 0;
    uint8_t *out = jpeg_alloc_encoder_mem(out_cap, &out_cfg, &out_sz);
    if (!out) {
        free(in);
        web_hook_display_pause(false);
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
        return web_send_status(req, 500, "error", "JPEG encode failed", NULL);
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=\"screenshot.jpg\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t r = httpd_resp_send(req, (const char *)out, jpg_size);
    free(out);
    return r;
}
