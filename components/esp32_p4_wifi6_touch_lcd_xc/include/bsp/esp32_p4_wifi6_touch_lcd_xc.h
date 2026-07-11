#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "bsp/config.h"
#include "bsp/display.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "lvgl.h"
#include "esp_lvgl_port.h"
#endif // BSP_CONFIG_NO_GRAPHIC_LIB == 0

/**************************************************************************************************
 *  BSP Capabilities
 *
 *  This is the Waveshare ESP32-P4-WIFI6-Touch-LCD-XC round-panel board adapted for the
 *  AllSky-Display firmware. Audio, SD card and USB host are present on the hardware but are
 *  intentionally not wired here (not used by this firmware) to keep the dependency surface small.
 **************************************************************************************************/
#define BSP_CAPS_DISPLAY        1
#define BSP_CAPS_TOUCH          1
#define BSP_CAPS_BUTTONS        0
#define BSP_CAPS_AUDIO          0
#define BSP_CAPS_AUDIO_SPEAKER  0
#define BSP_CAPS_AUDIO_MIC      0
#define BSP_CAPS_SDCARD         0
#define BSP_CAPS_IMU            0

/**************************************************************************************************
 *  Pinout (Waveshare ESP32-P4-WIFI6-Touch-LCD-XC)
 **************************************************************************************************/
/* I2C (shared bus: GT911 touch) */
#define BSP_I2C_SCL           (GPIO_NUM_8)
#define BSP_I2C_SDA           (GPIO_NUM_7)

/* Display */
#define BSP_LCD_BACKLIGHT     (GPIO_NUM_26)
#define BSP_LCD_RST           (GPIO_NUM_27)
#define BSP_LCD_TOUCH_RST     (GPIO_NUM_23)
#define BSP_LCD_TOUCH_INT     (GPIO_NUM_NC)

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * I2C interface (GT911 touch controller)
 **************************************************************************************************/
#define BSP_I2C_NUM     CONFIG_BSP_I2C_NUM

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 * SPIFFS
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT      CONFIG_BSP_SPIFFS_MOUNT_POINT

esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);

/**************************************************************************************************
 * LCD interface (LVGL via esp_lvgl_port)
 *
 * LVGL is NOT thread safe: take the LVGL mutex with bsp_display_lock() before any lv_* call and
 * release it with bsp_display_unlock().
 **************************************************************************************************/
#define BSP_LCD_PIXEL_CLOCK_MHZ     (80)

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)

#define BSP_LCD_DRAW_BUFF_SIZE     (BSP_LCD_H_RES * 50) // Draw buffer size in pixels
#define BSP_LCD_DRAW_BUFF_DOUBLE   (0)

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    lvgl_port_cfg_t lvgl_port_cfg;  /*!< LVGL port configuration */
    uint32_t        buffer_size;    /*!< Size of the buffer for the screen in pixels */
    bool            double_buffer;  /*!< True, if should be allocated two buffers */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated LVGL buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated LVGL buffer will be in PSRAM */
        unsigned int sw_rotate: 1;   /*!< Use software rotation (unavailable under avoid-tear) */
    } flags;
} bsp_display_cfg_t;

/**
 * @brief Initialize display + touch + LVGL with default configuration.
 *
 * @return Pointer to LVGL display or NULL on error.
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Initialize display + touch + LVGL with an explicit configuration.
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg);

/**
 * @brief Get the LVGL input device (touch) created by bsp_display_start().
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Get the underlying esp_lcd panel handle.
 */
esp_lcd_panel_handle_t bsp_display_get_panel_handle(void);

/**
 * @brief Take the LVGL mutex.
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give the LVGL mutex.
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate the LVGL display.
 */
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);

#endif // BSP_CONFIG_NO_GRAPHIC_LIB == 0

#ifdef __cplusplus
}
#endif
