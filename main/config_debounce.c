#include "config_debounce.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "config_debounce";

static void debounce_timer_cb(void *arg)
{
    config_debounce_t *d = (config_debounce_t *)arg;
    if (d && d->flush) {
        d->flush(d->arg);
    }
}

esp_err_t config_debounce_init(config_debounce_t *d, uint32_t delay_ms,
                               void (*flush)(void *arg), void *arg)
{
    if (!d || !flush || delay_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(d, 0, sizeof(*d));
    d->delay_ms = delay_ms;
    d->flush = flush;
    d->arg = arg;

    const esp_timer_create_args_t args = {
        .callback = debounce_timer_cb,
        .arg = d,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "cfg_debounce",
    };
    esp_err_t err = esp_timer_create(&args, &d->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t config_debounce_touch(config_debounce_t *d)
{
    if (!d || !d->timer) {
        return ESP_ERR_INVALID_STATE;
    }
    /* Restart semantics: stop a pending fire, then arm from now. */
    esp_timer_stop(d->timer);
    return esp_timer_start_once(d->timer, (uint64_t)d->delay_ms * 1000ULL);
}

void config_debounce_flush_now(config_debounce_t *d)
{
    if (!d) {
        return;
    }
    if (d->timer) {
        esp_timer_stop(d->timer);
    }
    if (d->flush) {
        d->flush(d->arg);
    }
}

void config_debounce_deinit(config_debounce_t *d)
{
    if (!d || !d->timer) {
        return;
    }
    esp_timer_stop(d->timer);
    esp_timer_delete(d->timer);
    d->timer = NULL;
}
