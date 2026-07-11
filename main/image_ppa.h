#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Thin wrapper over the PPA (Pixel-Processing Accelerator) SRM engine for RGB565
 * hardware scaling. A single SRM client is registered lazily and shared by the
 * decode-downscale and transform-upscale paths so the pipeline holds at most one
 * PPA client. Rotation is handled in software by the transform stage, so this
 * wrapper only scales (rotation angle 0).
 */

/**
 * @brief Eagerly create the PPA serialization mutex and register the SRM client.
 *
 * Call once during single-threaded init so the worker and loop tasks never race
 * on lazy creation. Safe to call more than once.
 */
esp_err_t image_ppa_init(void);

/**
 * @brief Hardware-scale an RGB565 image into a caller-provided destination.
 *
 * The destination must be 128-byte aligned PSRAM (DMA-capable, L2 cache line).
 * The source must be DMA-accessible (PSRAM) and cache-synced by the caller.
 *
 * @param src         Source RGB565 buffer.
 * @param src_w       Source content width in pixels.
 * @param src_h       Source content height in pixels.
 * @param src_stride  Source stride in pixels (0 = src_w).
 * @param dst         Pre-allocated 128-byte aligned destination buffer.
 * @param dst_cap     Capacity of @p dst in bytes.
 * @param dst_w       Target width in pixels.
 * @param dst_h       Target height in pixels.
 * @return ESP_OK on success; ESP_ERR_* otherwise (caller falls back to software).
 */
esp_err_t image_ppa_scale(const uint16_t *src, uint32_t src_w, uint32_t src_h,
                          uint32_t src_stride, uint16_t *dst, size_t dst_cap,
                          uint32_t dst_w, uint32_t dst_h);

#ifdef __cplusplus
}
#endif
