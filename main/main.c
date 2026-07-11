/*
 * AllSky-Display (ESP-IDF) application entry point.
 *
 * Boots the subsystems in dependency order: log capture and crash forensics,
 * configuration, display + backlight, the supervised system core, then the
 * network stack (captive provisioning on first boot, else station mode), the
 * moon renderer, image pipeline, touch input, MQTT/Home Assistant, and the serial
 * console. The main task then runs the cooperative housekeeping loop that ticks
 * the watchdog/monitor, the WiFi state machine, and time sync, and brings the web
 * server + MQTT online on the WiFi up-edge.
 */

#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include "display_defs.h"
#include "app_config.h"
#include "system_log_capture.h"
#include "system_crash_log.h"
#include "system_tasks.h"
#include "system_health.h"
#include "system_serial.h"
#include "network_wifi.h"
#include "network_time.h"
#include "network_portal.h"
#include "image_pipeline.h"
#include "moon_source.h"
#include "input_touch.h"
#include "mqtt_ha.h"
#include "brightness_ctrl.h"
#include "web_server.h"
#include "integration_hooks.h"

/* Web-team main-loop backstop (auto-resume tuning after idle). */
void web_editing_tick(void);

#define ALLSKY_FW_NAME  "AllSky-Display"

static const char *TAG = "allsky_main";

static void halt_forever(const char *why)
{
    ESP_LOGE(TAG, "FATAL: %s - halting (unsupervised loop refused)", why);
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* First-boot provisioning: run the captive portal until credentials are saved
 * (then reboot into station mode) or the setup window elapses. The watchdog is
 * fed explicitly because the supervised main loop is not reached in this path. */
static void run_provisioning_portal(void)
{
    ESP_LOGW(TAG, "WiFi not provisioned; bringing up setup portal");
    if (network_portal_start() != ESP_OK) {
        ESP_LOGE(TAG, "setup portal failed to start");
    }

    const int64_t timeout_ms = 300000;
    const int64_t start_ms = esp_timer_get_time() / 1000;

    for (;;) {
        if (esp_task_wdt_status(NULL) == ESP_OK) {
            esp_task_wdt_reset();
        }

        if (network_portal_is_configured()) {
            ESP_LOGI(TAG, "credentials saved; rebooting into station mode");
            network_portal_stop();
            vTaskDelay(pdMS_TO_TICKS(2000));
            crash_log_prepare_reboot();
            esp_restart();
        }

        if ((esp_timer_get_time() / 1000 - start_ms) > timeout_ms) {
            ESP_LOGW(TAG, "provisioning window elapsed; rebooting");
            crash_log_prepare_reboot();
            esp_restart();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    /* NVS first: config store, crash logger and WiFi credentials all rely on it. */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Boot log capture must precede crash logging so the vprintf chain is intact
     * when the crash logger prints its banner. */
    log_capture_init();
    crash_log_init();

    /* Persisted configuration (defaults + stored values). Must precede display
     * bring-up so the runtime panel selection is known before buffers are sized. */
    ESP_ERROR_CHECK(app_config_init());

    /* Select the active panel profile from persisted config, then bring up the
     * display + touch + LVGL. Both the app profile table and the BSP get the same
     * type id, before bsp_display_start sizes the draw buffers. */
    int disp_type = (int)app_config_get_disp_type();
    allsky_panel_set_type(disp_type);
    ESP_ERROR_CHECK(bsp_display_set_panel_type(disp_type));

    allsky_panel_profile_t panel = allsky_panel_profile();
    ESP_LOGI(TAG, "Starting %s, panel %s (type %d) %dx%d",
             ALLSKY_FW_NAME, panel.name, panel.type_id, panel.width, panel.height);

    lv_display_t *disp = bsp_display_start();
    if (disp == NULL) {
        halt_forever("display bring-up failed");
    }

    bsp_display_lock(0);
    bsp_display_rotate(disp, LV_DISPLAY_ROTATION_180);
    bsp_display_unlock();

    bsp_display_backlight_on();

    /* Backlight policy funnel; after the BSP has configured the LEDC channel. */
    ESP_ERROR_CHECK(brightness_init());

    /* Supervised system core: watchdog + monitor + health, subscribing this main
     * task to the watchdog. On failure we must halt rather than run unsupervised. */
    if (system_core_init() != ESP_OK) {
        halt_forever("system core init failed");
    }

    /* Live-value providers for the device-health analyzer. */
    static const system_health_providers_t health_providers = {
        .wifi_connected     = network_wifi_is_connected,
        .wifi_rssi          = integ_wifi_rssi,
        .wifi_bssid         = integ_wifi_bssid,
        .display_brightness = brightness_get,
    };
    system_health_set_providers(&health_providers);

    /* TCP/IP stack + default event loop (network init also does this defensively). */
    esp_err_t e = esp_netif_init();
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "esp_netif_init: %s", esp_err_to_name(e));
    }
    e = esp_event_loop_create_default();
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "esp_event_loop_create_default: %s", esp_err_to_name(e));
    }

    /* First boot with no credentials: run the setup portal (this path reboots). */
    if (!app_config_get_wifi_prov()) {
        run_provisioning_portal();
        return;   /* not reached */
    }

    /* Station mode: apply TZ, bring up the radio, then start connecting. */
    network_time_init();
    if (network_wifi_init() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed");
    } else {
        vTaskDelay(pdMS_TO_TICKS(500));
        network_wifi_connect_start();
    }

    /* Moon renderer: prepare the texture and the interactive drag driver. */
    moon_source_init();
    moon_interactive_init(integ_moon_blit, integ_moon_settle);

    /* Image pipeline: allocate buffers + LVGL sink, then spawn the tasks. */
    static const image_pipeline_deps_t pipeline_deps = {
        .wifi_connected  = network_wifi_is_connected,
        .ota_in_progress = web_ota_in_progress,
    };
    esp_err_t ip = image_pipeline_init(&pipeline_deps);
    if (ip != ESP_OK) {
        ESP_LOGE(TAG, "image pipeline init failed: %s", esp_err_to_name(ip));
    } else if (image_pipeline_start() != ESP_OK) {
        ESP_LOGE(TAG, "image pipeline start failed");
    }

    /* Touch gestures: single tap advances (suppressed on moon), double tap toggles
     * single-image-refresh mode. */
    static const input_gesture_hooks_t gesture_hooks = {
        .single_tap = integ_single_tap,
        .double_tap = image_pipeline_toggle_single_refresh,
    };
    input_touch_init(&gesture_hooks);

    /* MQTT + Home Assistant. Started on the WiFi up-edge in the loop below. */
    static const mqtt_ha_hooks_t mqtt_hooks = {
        .apply_brightness     = brightness_apply,
        .get_brightness       = brightness_get,
        .image_index_changed  = integ_image_index_changed,
        .before_reboot        = crash_log_prepare_reboot,
        .on_reconnect_attempt = system_health_record_mqtt_reconnect,
        .get_rssi             = integ_mqtt_rssi,
        .ota_in_progress      = web_ota_in_progress,
        .wifi_connected       = network_wifi_is_connected,
    };
    mqtt_ha_init(&mqtt_hooks);

    /* Serial single-key command console. */
    static const system_serial_hooks_t serial_hooks = {
        .apply_brightness   = brightness_apply,
        .get_brightness     = brightness_get,
        .next_image         = image_pipeline_next_image,
        .force_refresh      = image_pipeline_request_refresh,
        .rerender_current   = image_pipeline_notify_edit,
        .restart_web_server = integ_restart_web_server,
        .web_status         = integ_serial_web_status,
        .network_info       = integ_serial_network_info,
        .ppa_info           = integ_serial_ppa_info,
    };
    system_serial_init(&serial_hooks);
    system_serial_start();

    ESP_LOGI(TAG, "Init complete; entering main loop");

    /* ---- Cooperative housekeeping loop (system.md 2.5) ------------------- */
    bool    wifi_prev = false;
    bool    image_valid_marked = false;
    int64_t last_edit_ms = 0;

    for (;;) {
        system_core_loop_tick();
        network_wifi_update();
        network_time_update();

        bool wifi_now = network_wifi_is_connected();
        if (wifi_now && !wifi_prev) {
            ESP_LOGI(TAG, "WiFi connected; starting web server + MQTT");
            web_server_start();
            mqtt_ha_start();
            mqtt_ha_rest_start();
        }
        wifi_prev = wifi_now;

        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - last_edit_ms >= 1000) {
            web_editing_tick();
            last_edit_ms = now_ms;
        }

        /* Confirm the running OTA image only after the pipeline has proven
         * healthy by decoding and presenting its first frame. This is the sole
         * rollback-cancel gate: an image that boots but cannot join WiFi or
         * produce a frame is never confirmed, so the bootloader reverts it. */
        if (!image_valid_marked && image_pipeline_has_first_image()) {
            system_core_mark_image_valid();
            image_valid_marked = true;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
