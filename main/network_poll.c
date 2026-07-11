/**
 * @file network_poll.c
 * @brief Shared poll-loop skeleton. See network_poll.h for scope.
 */

#include "network_poll.h"
#include "network_poll_backoff.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "net_poll";

static inline bool gate_blocked(const network_poll_spec_t *spec) {
    if (spec->ota_gate && *spec->ota_gate) return true;
    if (spec->active_gate && !atomic_load(spec->active_gate)) return true;
    return false;
}

void network_poll_loop_run(const network_poll_spec_t *spec, void *arg) {
    if (!spec || !spec->poll_once || !spec->interval_ms) {
        ESP_LOGE(TAG, "invalid poll spec");
        return;
    }

    if (spec->wifi_group) {
        xEventGroupWaitBits(spec->wifi_group, spec->wifi_bits, pdFALSE, pdFALSE, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "%s: starting poll loop", spec->name);

    uint32_t backoff_ms = 0;

    while (1) {
        /* Suspend during OTA or while gated inactive; a task notify wakes us to re-check. */
        while (gate_blocked(spec)) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        }

        bool ok = spec->poll_once(arg);

        uint32_t wait_ms;
        if (ok) {
            backoff_ms = 0;
            wait_ms = spec->interval_ms(arg);
        } else if (spec->backoff_initial_ms == 0) {
            wait_ms = spec->interval_ms(arg);
        } else {
            backoff_ms = network_poll_backoff_next(backoff_ms, spec->backoff_initial_ms,
                                                   spec->backoff_max_ms);
            wait_ms = backoff_ms;
            ESP_LOGW(TAG, "%s: poll failed, retrying in %u ms", spec->name, (unsigned)wait_ms);
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
    }
}
