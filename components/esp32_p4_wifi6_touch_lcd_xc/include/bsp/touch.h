#pragma once
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create new touchscreen (GT911 on the shared I2C bus)
 *
 * The touch coordinate range is taken from the panel selected at runtime by
 * bsp_display_set_panel_type() (bsp_display_get_h_res / _v_res). Free with esp_lcd_touch_del(tp).
 *
 * @param[out] ret_touch esp_lcd_touch touchscreen handle
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd_touch failure
 */
esp_err_t bsp_touch_new(esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
