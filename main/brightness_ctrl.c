#include "brightness_ctrl.h"
#include "app_config.h"
#include "mqtt_ha.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"

static const char *TAG = "brightness";

static int s_current = 50;   /* cached percentage, mirrors the BSP duty */

static int clamp_pct(int pct)
{
    if (pct < 0) {
        return 0;
    }
    if (pct > 100) {
        return 100;
    }
    return pct;
}

void brightness_apply(int pct)
{
    pct = clamp_pct(pct);
    esp_err_t err = bsp_display_brightness_set(pct);
    if (err != ESP_OK) {
        /* Brightness control unavailable (PWM not initialized): silent per spec. */
        ESP_LOGD(TAG, "backlight set %d%% failed: %s", pct, esp_err_to_name(err));
        return;
    }
    s_current = pct;
}

int brightness_get(void)
{
    return s_current;
}

void brightness_apply_config_default(void)
{
    brightness_apply((int)app_config_get_def_bright());
}

esp_err_t brightness_init(void)
{
    brightness_apply_config_default();
    ESP_LOGI(TAG, "backlight brightness %d%%", s_current);
    return ESP_OK;
}

void brightness_step(int delta)
{
    brightness_apply(s_current + delta);
    ESP_LOGI(TAG, "brightness stepped to %d%%", s_current);
}

void brightness_reapply_active_mode(void)
{
    if (app_config_get_use_ha_rest()) {
        int b = 0;
        if (mqtt_ha_rest_last(NULL, &b)) {
            brightness_apply(b);
        } else {
            brightness_apply_config_default();
        }
    } else if (app_config_get_bright_auto()) {
        mqtt_ha_publish_state();
    } else {
        brightness_apply_config_default();
    }
}

void brightness_set_auto_mode(bool on)
{
    app_config_set_bright_auto(on);
    if (on) {
        app_config_set_use_ha_rest(false);
    }
    app_config_save();
}

void brightness_set_ha_rest(bool on)
{
    app_config_set_use_ha_rest(on);
    if (on) {
        app_config_set_bright_auto(false);
    }
    app_config_save();
}
