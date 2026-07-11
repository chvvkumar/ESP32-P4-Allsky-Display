#pragma once
#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"
#include "sdkconfig.h"

/* LCD color formats */
#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

/* LCD display color format */
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB888)
#else
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
#endif
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (0)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (ESP_LCD_COLOR_SPACE_RGB)

/*
 * Panel resolution and the one differing byte of the JD9365 init sequence (page-1 reg 0x40)
 * are selected at RUNTIME. Both round panels are supported from a single binary; the active
 * panel is chosen by bsp_display_set_panel_type() which MUST be called before bsp_display_new /
 * bsp_display_start. The Kconfig ALLSKY_PANEL_* choice is only the first-boot default fed into
 * persisted config; the BSP no longer reads CONFIG_ALLSKY_PANEL_* directly.
 *
 *   type id 1 -> 3.4" round, 800x800, reg 0x40 = 0x00 (default)
 *   type id 2 -> 4.0" round, 720x720, reg 0x40 = 0x04
 */
#define BSP_LCD_MIPI_DSI_LANE_BITRATE_MBPS (1500)

#define BSP_LCD_MIPI_DSI_LANE_NUM          (2)    // 2 data lanes
#define BSP_MIPI_DSI_PHY_PWR_LDO_CHAN       (3)  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV (2500)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 */
typedef struct {
    int dummy;
} bsp_display_config_t;

/**
 * @brief BSP display return handles
 */
typedef struct {
    esp_lcd_dsi_bus_handle_t    mipi_dsi_bus;  /*!< MIPI DSI bus handle */
    esp_lcd_panel_io_handle_t   io;            /*!< ESP LCD IO handle */
    esp_lcd_panel_handle_t      panel;         /*!< ESP LCD panel (color) handle */
    esp_lcd_panel_handle_t      control;       /*!< ESP LCD panel (control) handle */
} bsp_lcd_handles_t;

/**
 * @brief Select the active panel type (1 = 3.4" 800x800, 2 = 4.0" 720x720).
 *
 * Sets the DSI panel resolution, the JD9365 page-1 reg 0x40 init byte, and the LVGL draw-buffer
 * size used by the subsequent bring-up. MUST be called before bsp_display_new /
 * bsp_display_new_with_handles / bsp_display_start. Unknown ids fall back to type 1. Because the
 * draw buffers are sized once at start, changing the type after start requires a reboot.
 */
esp_err_t bsp_display_set_panel_type(int type_id);

/**
 * @brief Active panel resolution set by bsp_display_set_panel_type() (defaults to 800x800).
 */
int bsp_display_get_h_res(void);
int bsp_display_get_v_res(void);

/**
 * @brief Create new display panel (reset + init only; backlight left off).
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Create new display panel, returning all esp_lcd handles.
 */
esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles);

/**
 * @brief Initialize display's brightness (LEDC PWM backlight).
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief Set display's brightness in [%].
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Get the last brightness value set, in [%].
 */
int bsp_display_brightness_get(void);

/**
 * @brief Turn on display backlight.
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn off display backlight.
 */
esp_err_t bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
