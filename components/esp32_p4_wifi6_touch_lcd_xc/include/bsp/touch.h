#pragma once
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create new touchscreen (GT911 on the shared I2C bus)
 *
 * The touch coordinate range is taken from the selected panel profile
 * (BSP_LCD_H_RES / BSP_LCD_V_RES). Free with esp_lcd_touch_del(tp).
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
