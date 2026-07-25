#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Metadata for a decoded RGB565 frame living in a pipeline full-image buffer. */
typedef struct {
    uint16_t width;      /* content width in pixels */
    uint16_t height;     /* content height in pixels */
    uint16_t stride_px;  /* row stride in pixels (>= width; MCU alignment on the direct path) */
} image_frame_meta_t;

/**
 * @brief Create the shared JPEG decoder engine once, ahead of any decode.
 *
 * Allocates the engine's DMA descriptors while internal heap is still plentiful
 * and keeps the handle for the process lifetime. Call once at startup, before
 * the web server / MQTT come up. Safe to call more than once (no-op once ready).
 * @return ESP_OK when the engine is ready; ESP_FAIL / ESP_ERR_* on failure
 *         (image_decode() retries creation on demand).
 */
esp_err_t image_decode_init(void);

/**
 * @brief Hardware-decode a baseline JPEG into a pre-allocated RGB565 buffer.
 *
 * Reads the native dimensions, selects the smallest downscale divisor d in
 * {1,2,4,8} such that the decoded content fits @p dst_cap and neither dimension
 * exceeds @p max_dim, then writes the result into @p dst. Divisor 1 decodes
 * straight into @p dst (MCU-aligned stride). Larger divisors decode into a
 * transient native buffer and PPA-downscale into @p dst (tightly packed).
 * Grayscale JPEGs are expanded to RGB565.
 *
 * @param dst      Pre-allocated 128-byte aligned PSRAM buffer (DMA capable).
 * @param dst_cap  Capacity of @p dst in bytes.
 * @param max_dim  Maximum allowed content dimension (e.g. 1448).
 * @param wdt_feed Optional watchdog-feed callback invoked around blocking work.
 * @param out      Receives the decoded frame geometry on success.
 * @return ESP_OK on success; ESP_FAIL / ESP_ERR_* on any decode failure
 *         (progressive/unsupported JPEG, allocation failure, HW error).
 */
esp_err_t image_decode(const uint8_t *jpg, size_t jpg_len,
                       uint16_t *dst, size_t dst_cap, uint16_t max_dim,
                       void (*wdt_feed)(void), image_frame_meta_t *out);

#ifdef __cplusplus
}
#endif
