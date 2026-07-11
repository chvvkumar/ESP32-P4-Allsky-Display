/*
 * AllSky-Display (ESP-IDF rewrite) - Phase B scaffold entry point.
 *
 * Brings up the BSP display + GT911 touch + LVGL (esp_lvgl_port) and shows a centered
 * label reporting the firmware name, selected panel variant and resolution. This proves the
 * board bring-up path end to end. The panel is selected at runtime from persisted config
 * (NVS disp_type); both round panels are served by this single binary.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "display_defs.h"
#include "app_config.h"

#define ALLSKY_FW_NAME "AllSky-Display"

static const char *TAG = "allsky_main";

static void show_test_screen(void)
{
    allsky_panel_profile_t panel = allsky_panel_profile();

    /* All lv_* calls must run under the LVGL mutex. */
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Could not take LVGL lock");
        return;
    }

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_HOR_RES - 40);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text_fmt(label,
                          "%s\n\nESP-IDF rewrite\nPanel %s (type %d)\n%d x %d",
                          ALLSKY_FW_NAME, panel.name, panel.type_id,
                          panel.width, panel.height);
    lv_obj_center(label);

    bsp_display_unlock();

    ESP_LOGI(TAG, "%s test screen up: panel %s (type %d) %dx%d",
             ALLSKY_FW_NAME, panel.name, panel.type_id, panel.width, panel.height);
}

void app_main(void)
{
    /* NVS is initialized early so later phases (config store) can rely on it. */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Load persisted configuration (defaults + stored values). Must precede display bring-up so
     * the runtime panel selection (NVS disp_type) is known before buffers are sized. */
    ESP_ERROR_CHECK(app_config_init());

    /* Select the active panel profile from persisted config (Kconfig ALLSKY_PANEL_* is only the
     * first-boot default baked into that value). Both the app-side profile table and the BSP are
     * fed the same type id; this MUST happen before bsp_display_start sizes the draw buffers. */
    int disp_type = (int)app_config_get_disp_type();
    allsky_panel_set_type(disp_type);
    ESP_ERROR_CHECK(bsp_display_set_panel_type(disp_type));

    allsky_panel_profile_t panel = allsky_panel_profile();
    ESP_LOGI(TAG, "Starting %s, panel %s (type %d) %dx%d",
             ALLSKY_FW_NAME, panel.name, panel.type_id, panel.width, panel.height);

    /* Bring up display + touch + LVGL, then enable the backlight. */
    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display bring-up failed");
        return;
    }

    /* Fixed 180-degree rotation per the display-driver spec. */
    bsp_display_lock(0);
    bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
    bsp_display_unlock();

    show_test_screen();

    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Init complete");
}
