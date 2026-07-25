/**
 * @file image_decode.c
 * @brief Baseline-JPEG hardware decode with downscale-divisor selection.
 */

#include "image_decode.h"
#include "image_ppa.h"

#include <string.h>

#include "driver/jpeg_decode.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "img_decode";

/* The JPEG decoder engine is created once at startup and reused for the process
 * lifetime, so its DMA descriptors are allocated while internal heap is still
 * plentiful and can never be refused later under chronic low-memory pressure.
 * All decode calls run on the single image-pipeline worker task, so the handle
 * needs no locking. */
static jpeg_decoder_handle_t s_decoder;

static inline uint32_t align16(uint32_t v)
{
    return (v + 15u) & ~15u;
}

/* Choose the smallest divisor in {1,2,4,8} whose decoded content fits. Returns 0
 * if even 1/8 will not fit. Fills the storage byte count for the chosen divisor. */
static int select_divisor(uint32_t w, uint32_t h, size_t cap, uint16_t max_dim,
                          size_t *store_bytes_out)
{
    static const int divisors[] = { 1, 2, 4, 8 };
    for (size_t i = 0; i < sizeof(divisors) / sizeof(divisors[0]); ++i) {
        int d = divisors[i];
        uint32_t cw = w / d;
        uint32_t ch = h / d;
        if (cw == 0 || ch == 0) {
            continue;
        }
        if (cw > max_dim || ch > max_dim) {
            continue;
        }
        size_t store;
        if (d == 1) {
            store = (size_t)align16(w) * align16(h) * 2; /* decoded in place, MCU-aligned */
        } else {
            store = (size_t)cw * ch * 2;                  /* PPA output, tightly packed */
        }
        if (store <= cap) {
            *store_bytes_out = store;
            return d;
        }
    }
    return 0;
}

/* Create the shared decoder engine once. Safe to call repeatedly; a no-op once
 * the handle exists. Returns ESP_OK when the engine is ready. */
esp_err_t image_decode_init(void)
{
    if (s_decoder) {
        return ESP_OK;
    }
    jpeg_decode_engine_cfg_t engine_cfg = {
        .intr_priority = 0,
        .timeout_ms = 5000,
    };
    esp_err_t err = jpeg_new_decoder_engine(&engine_cfg, &s_decoder);
    if (err != ESP_OK || !s_decoder) {
        s_decoder = NULL;
        ESP_LOGE(TAG, "decoder engine create failed: %s", esp_err_to_name(err));
        return err != ESP_OK ? err : ESP_FAIL;
    }
    return ESP_OK;
}

/* Run one HW decode of jpg -> out (RGB565 or GRAY). Returns ESP_OK on success.
 * Uses the shared engine created by image_decode_init(); called only from the
 * single image-pipeline worker task, so no locking is needed. */
static esp_err_t hw_decode(const uint8_t *jpg, size_t jpg_len, bool gray,
                           uint8_t *out, size_t out_cap, void (*wdt_feed)(void))
{
    if (!s_decoder) {
        /* Engine creation failed at init (or was never run); retry now so one
         * boot-time failure is not permanent. */
        esp_err_t init_err = image_decode_init();
        if (init_err != ESP_OK) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    jpeg_decode_cfg_t decode_cfg = {
        .output_format = gray ? JPEG_DECODE_OUT_FORMAT_GRAY : JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
    };
    uint32_t out_size = 0;
    if (wdt_feed) {
        wdt_feed();
    }
    esp_err_t err = jpeg_decoder_process(s_decoder, &decode_cfg, jpg, jpg_len,
                                         out, out_cap, &out_size);
    if (wdt_feed) {
        wdt_feed();
    }

    if (err != ESP_OK || out_size == 0) {
        ESP_LOGW(TAG, "decode failed: %s", esp_err_to_name(err));
        return err != ESP_OK ? err : ESP_FAIL;
    }
    return ESP_OK;
}

/* Expand a GRAY (1 byte/pixel) buffer to RGB565 in dst. */
static void gray_to_rgb565(const uint8_t *gray, uint16_t *dst, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t g = gray[i];
        dst[i] = (uint16_t)(((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3));
    }
}

esp_err_t image_decode(const uint8_t *jpg, size_t jpg_len,
                       uint16_t *dst, size_t dst_cap, uint16_t max_dim,
                       void (*wdt_feed)(void), image_frame_meta_t *out)
{
    if (!jpg || jpg_len == 0 || !dst || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    jpeg_decode_picture_info_t pic = {0};
    esp_err_t err = jpeg_decoder_get_info(jpg, jpg_len, &pic);
    if (err != ESP_OK || pic.width == 0 || pic.height == 0) {
        ESP_LOGW(TAG, "get_info failed: %s", esp_err_to_name(err));
        return err != ESP_OK ? err : ESP_FAIL;
    }

    bool gray = (pic.sample_method == JPEG_DOWN_SAMPLING_GRAY);
    uint32_t w = pic.width, h = pic.height;
    uint32_t aw = align16(w), ah = align16(h);

    size_t store_bytes = 0;
    int d = select_divisor(w, h, dst_cap, max_dim, &store_bytes);
    if (d == 0) {
        ESP_LOGW(TAG, "image %ux%u does not fit even at 1/8", (unsigned)w, (unsigned)h);
        return ESP_ERR_INVALID_SIZE;
    }

    /* Native MCU-aligned RGB565 storage requirement (decode target). */
    size_t native_rgb_bytes = (size_t)aw * ah * 2;

    if (d == 1 && !gray) {
        /* Decode straight into the destination buffer at MCU-aligned stride. */
        if (native_rgb_bytes > dst_cap) {
            return ESP_ERR_INVALID_SIZE;
        }
        err = hw_decode(jpg, jpg_len, false, (uint8_t *)dst, dst_cap, wdt_feed);
        if (err != ESP_OK) {
            return err;
        }
        /* JPEG engine DMA-wrote dst; invalidate so CPU compose reads fresh data. */
        esp_cache_msync(dst, native_rgb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        out->width = (uint16_t)w;
        out->height = (uint16_t)h;
        out->stride_px = (uint16_t)aw;
        return ESP_OK;
    }

    /* All other paths decode into a transient native buffer first. */
    size_t decode_bytes = gray ? (size_t)aw * ah : native_rgb_bytes;
    uint8_t *decode_buf = heap_caps_aligned_calloc(128, 1, decode_bytes, MALLOC_CAP_SPIRAM);
    if (!decode_buf) {
        ESP_LOGW(TAG, "native decode buffer alloc failed (%u bytes)", (unsigned)decode_bytes);
        return ESP_ERR_NO_MEM;
    }

    err = hw_decode(jpg, jpg_len, gray, decode_buf, decode_bytes, wdt_feed);
    if (err != ESP_OK) {
        heap_caps_free(decode_buf);
        return err;
    }

    /* Establish a native RGB565 source (expand gray if needed). */
    uint16_t *native_rgb;
    uint8_t *native_owned = NULL; /* buffer to free (if distinct from decode_buf) */
    if (gray) {
        native_rgb = heap_caps_aligned_calloc(128, 1, native_rgb_bytes, MALLOC_CAP_SPIRAM);
        if (!native_rgb) {
            ESP_LOGW(TAG, "gray->rgb buffer alloc failed");
            heap_caps_free(decode_buf);
            return ESP_ERR_NO_MEM;
        }
        gray_to_rgb565(decode_buf, native_rgb, aw * ah);
        esp_cache_msync(native_rgb, native_rgb_bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        heap_caps_free(decode_buf);
        native_owned = (uint8_t *)native_rgb;
    } else {
        native_rgb = (uint16_t *)decode_buf;
        native_owned = decode_buf;
    }

    if (d == 1) {
        /* gray at native size: copy content rows (stride aw) into dst, tightly
         * packed at width w. */
        for (uint32_t y = 0; y < h; ++y) {
            memcpy(dst + (size_t)y * w, native_rgb + (size_t)y * aw, (size_t)w * 2);
        }
        esp_cache_msync(dst, (size_t)w * h * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        out->width = (uint16_t)w;
        out->height = (uint16_t)h;
        out->stride_px = (uint16_t)w;
        heap_caps_free(native_owned);
        return ESP_OK;
    }

    /* d > 1: PPA-downscale the native content (stride aw) into dst. */
    uint32_t cw = w / d, ch = h / d;
    if (wdt_feed) {
        wdt_feed();
    }
    err = image_ppa_scale(native_rgb, w, h, aw, dst, dst_cap, cw, ch);
    if (wdt_feed) {
        wdt_feed();
    }
    heap_caps_free(native_owned);
    if (err != ESP_OK) {
        return err;
    }

    /* PPA DMA-wrote dst; invalidate so CPU compose reads fresh data. */
    size_t sync = ((size_t)cw * ch * 2 + 127) & ~(size_t)127;
    if (sync > dst_cap) {
        sync = dst_cap;
    }
    esp_cache_msync(dst, sync, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

    out->width = (uint16_t)cw;
    out->height = (uint16_t)ch;
    out->stride_px = (uint16_t)cw;
    ESP_LOGI(TAG, "decoded %ux%u -> %ux%u (1/%d)", (unsigned)w, (unsigned)h,
             (unsigned)cw, (unsigned)ch, d);
    return ESP_OK;
}
