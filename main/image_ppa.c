/**
 * @file image_ppa.c
 * @brief Shared PPA SRM RGB565 scaler for the image pipeline.
 */

#include "image_ppa.h"

#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "img_ppa";

static ppa_client_handle_t s_srm_client;
static SemaphoreHandle_t   s_ppa_mtx;   /* serializes the shared client across tasks */

static esp_err_t ensure_client(void)
{
    if (!s_ppa_mtx) {
        s_ppa_mtx = xSemaphoreCreateMutex();
        if (!s_ppa_mtx) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_srm_client) {
        return ESP_OK;
    }
    ppa_client_config_t cfg = {
        .oper_type = PPA_OPERATION_SRM,
        .max_pending_trans_num = 1,
    };
    esp_err_t err = ppa_register_client(&cfg, &s_srm_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "PPA SRM client registration failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t image_ppa_init(void)
{
    return ensure_client();
}

esp_err_t image_ppa_scale(const uint16_t *src, uint32_t src_w, uint32_t src_h,
                          uint32_t src_stride, uint16_t *dst, size_t dst_cap,
                          uint32_t dst_w, uint32_t dst_h)
{
    if (!src || !dst || src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (src_stride == 0) {
        src_stride = src_w;
    }

    size_t needed = (size_t)dst_w * dst_h * 2;
    needed = (needed + 127) & ~(size_t)127;
    if (needed > dst_cap) {
        ESP_LOGW(TAG, "dst too small: need %u have %u", (unsigned)needed, (unsigned)dst_cap);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = ensure_client();
    if (err != ESP_OK) {
        return err;
    }

    ppa_srm_oper_config_t srm = {
        .in = {
            .buffer = src,
            .pic_w = src_stride,
            .pic_h = src_h,
            .block_w = src_w,
            .block_h = src_h,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = dst,
            .buffer_size = needed,
            .pic_w = dst_w,
            .pic_h = dst_h,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = (float)dst_w / (float)src_w,
        .scale_y = (float)dst_h / (float)src_h,
        .rgb_swap = false,
        .byte_swap = false,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    xSemaphoreTake(s_ppa_mtx, portMAX_DELAY);
    err = ppa_do_scale_rotate_mirror(s_srm_client, &srm);
    xSemaphoreGive(s_ppa_mtx);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PPA scale %ux%u -> %ux%u failed: %s",
                 (unsigned)src_w, (unsigned)src_h, (unsigned)dst_w, (unsigned)dst_h,
                 esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}
