/*
 * System-core orchestration. See system_tasks.h.
 */

#include "system_tasks.h"
#include "system_watchdog.h"
#include "system_monitor.h"
#include "system_health.h"
#include "app_config.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "sys_core";

static bool s_image_marked;

esp_err_t system_core_init(void)
{
    uint32_t wd_timeout = app_config_get_wd_timeout();
    esp_err_t err = system_watchdog_init(wd_timeout);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "watchdog init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = system_monitor_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "system monitor init failed: %s", esp_err_to_name(err));
        return err;
    }

    system_health_init();

    ESP_LOGI(TAG, "system core initialized (wd=%lu ms)", (unsigned long)wd_timeout);
    return ESP_OK;
}

void system_core_loop_tick(void)
{
    system_watchdog_force_feed();
    system_monitor_update();
}

void system_core_mark_image_valid(void)
{
    if (s_image_marked) {
        return;
    }
    s_image_marked = true;

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        return;
    }
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        return;
    }
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "running image marked valid; rollback cancelled");
        } else {
            ESP_LOGW(TAG, "mark image valid failed: %s", esp_err_to_name(err));
        }
    }
}
