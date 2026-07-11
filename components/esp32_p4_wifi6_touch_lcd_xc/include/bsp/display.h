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
 * Panel resolution and DSI lane bit-rate are selected by the single project-level
 * Kconfig choice ALLSKY_PANEL_* (main/Kconfig.projbuild), which is also the source
 * of truth for the app-side display profile (main/display_defs.h) and the one byte
 * that differs in the JD9365 init sequence (page-1 reg 0x40). This BSP intentionally
 * has NO panel-type choice of its own so the two can never disagree.
 *
 *   ALLSKY_PANEL_3_4 -> 3.4" round, 800x800 (default, type id 1)
 *   ALLSKY_PANEL_4_0 -> 4.0" round, 720x720 (type id 2)
 */
#if CONFIG_ALLSKY_PANEL_4_0
#define BSP_LCD_H_RES              (720)
#define BSP_LCD_V_RES              (720)
#else
#define BSP_LCD_H_RES              (800)
#define BSP_LCD_V_RES              (800)
#endif
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
