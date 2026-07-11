/*
 * Task watchdog management and watchdog-safe timing. See system_watchdog.h.
 */

#include "system_watchdog.h"

#include "esp_task_wdt.h"
#include "esp_log.h"

static const char *TAG = "sys_wdt";

static bool s_inited;

esp_err_t system_watchdog_init(uint32_t timeout_ms)
{
    /* Remove any watchdog configured by the startup code so the reconfigure below
     * cannot fail on a locked-in bootloader timeout. Ignored if none is active. */
    esp_task_wdt_deinit();

    esp_task_wdt_config_t cfg = {
        .timeout_ms    = timeout_ms,
        .idle_core_mask = 0,      /* idle tasks on both cores are not monitored */
        .trigger_panic = true,    /* starvation -> panic + reboot (captured by crash logger) */
    };

    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&cfg);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "task watchdog init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_inited = true;

    esp_err_t sub = esp_task_wdt_add(NULL);
    if (sub != ESP_OK && sub != ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "subscribe main task: %s", esp_err_to_name(sub));
    }

    ESP_LOGI(TAG, "task watchdog armed: timeout=%lu ms, action=panic+reboot",
             (unsigned long)timeout_ms);
    return ESP_OK;
}

esp_err_t system_watchdog_subscribe_current(void)
{
    esp_err_t err = esp_task_wdt_add(NULL);
    if (err == ESP_ERR_INVALID_ARG) {
        return ESP_OK;   /* already subscribed */
    }
    return err;
}

esp_err_t system_watchdog_unsubscribe_current(void)
{
    esp_err_t err = esp_task_wdt_delete(NULL);
    if (err == ESP_ERR_INVALID_ARG) {
        return ESP_OK;   /* not subscribed */
    }
    return err;
}

void system_watchdog_force_feed(void)
{
    if (!s_inited) {
        return;
    }
    /* Resets the calling task only; harmless (returns ESP_ERR_NOT_FOUND) if this
     * task is not subscribed. */
    esp_task_wdt_reset();
}

void system_watchdog_feed(void)
{
    system_watchdog_force_feed();
}

void system_watchdog_delay_ms(uint32_t ms)
{
    const uint32_t slice = 100;
    while (ms > 0) {
        uint32_t chunk = ms < slice ? ms : slice;
        system_watchdog_force_feed();
        vTaskDelay(pdMS_TO_TICKS(chunk));
        ms -= chunk;
    }
    system_watchdog_force_feed();
}

void system_watchdog_yield(void)
{
    system_watchdog_force_feed();
    taskYIELD();
    vTaskDelay(1);
}
