#include "web_internal.h"
#include "display_defs.h"
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_mac.h"

static const char *TAG = "web_api_read";

/* Preset catalog shared with the images SPA state (web-server.md 5.2 / 8.3). */
const web_preset_t g_web_presets[] = {
    { "sdo_aia_304", "Sun SDO/AIA 304A", "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0304.jpg", 1024, 88 },
    { "sdo_aia_171", "Sun SDO/AIA 171A", "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0171.jpg", 1024, 88 },
    { "sdo_aia_193", "Sun SDO/AIA 193A", "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0193.jpg", 1024, 88 },
    { "sdo_hmi_igr", "Sun SDO/HMI Continuum", "https://soho.nascom.nasa.gov/data/realtime/hmi_igr/1024/latest.jpg", 1024, 92 },
    { "sdo_hmi_mag", "Sun SDO/HMI Magnetogram", "https://soho.nascom.nasa.gov/data/realtime/hmi_mag/1024/latest.jpg", 1024, 92 },
    { "soho_c2", "Sun SOHO LASCO C2", "https://soho.nascom.nasa.gov/data/realtime/c2/1024/latest.jpg", 1024, 90 },
    { "soho_c3", "Sun SOHO LASCO C3", "https://soho.nascom.nasa.gov/data/realtime/c3/1024/latest.jpg", 1024, 90 },
    { "goes19_full", "Earth GOES-19 Full Disc", "https://cdn.star.nesdis.noaa.gov/GOES19/ABI/FD/GEOCOLOR/1808x1808.jpg", 1808, 96 },
};
const int g_web_preset_count = sizeof(g_web_presets) / sizeof(g_web_presets[0]);

static uint64_t uptime_ms(void) { return (uint64_t)(esp_timer_get_time() / 1000); }

static int current_brightness(void)
{
    /* Displayed brightness tracks the persisted default (live copy owned by the
     * display manager). */
    return (int)app_config_get_def_bright();
}

/* ---- GET /status --------------------------------------------------------- */
esp_err_t web_status_handler(httpd_req_t *req)
{
    web_net_status_t net;
    web_hook_net_status(&net);

    char ssid_esc[80] = "";
    web_json_escape(ssid_esc, sizeof(ssid_esc), net.connected ? net.ssid : "Not connected");

    char body[512];
    snprintf(body, sizeof(body),
        "{\"wifi_connected\":%s,\"wifi_ssid\":\"%s\",\"wifi_ip\":\"%s\","
        "\"wifi_rssi\":%d,\"mqtt_connected\":%s,\"free_heap\":%u,\"free_psram\":%u,"
        "\"uptime\":%llu,\"brightness\":%d}",
        net.connected ? "true" : "false", ssid_esc,
        net.connected ? net.ip : "0.0.0.0", net.rssi,
        web_hook_mqtt_connected() ? "true" : "false",
        (unsigned)esp_get_free_heap_size(),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned long long)uptime_ms(), current_brightness());
    return web_send_json(req, 200, body);
}

/* ---- GET /api/info ------------------------------------------------------- */
esp_err_t web_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) { httpd_resp_send_500(req); return ESP_FAIL; }

    allsky_panel_profile_t panel = allsky_panel_profile();
    web_net_status_t net;
    web_hook_net_status(&net);

    char sbuf[256];

    /* firmware */
    cJSON *fw = cJSON_AddObjectToObject(root, "firmware");
    cJSON_AddNumberToObject(fw, "size", web_fw_app_size());
    cJSON_AddNumberToObject(fw, "free_space", web_fw_free_space());
    web_fw_checksum_full(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(fw, "checksum", sbuf);
    cJSON_AddStringToObject(fw, "version", web_fw_version());

    /* system */
    esp_chip_info_t chip; esp_chip_info(&chip);
    uint32_t flash_size = 0; esp_flash_get_size(NULL, &flash_size);
    app_config_get_device_name(sbuf, sizeof(sbuf));
    cJSON *sys = cJSON_AddObjectToObject(root, "system");
    cJSON_AddStringToObject(sys, "device_name", sbuf);
    cJSON_AddNumberToObject(sys, "uptime", (double)uptime_ms());
    cJSON_AddNumberToObject(sys, "uptime_seconds", (double)(uptime_ms() / 1000));
    cJSON_AddNumberToObject(sys, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(sys, "total_heap", heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(sys, "min_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(sys, "free_psram", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(sys, "total_psram", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(sys, "min_free_psram", heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddNumberToObject(sys, "flash_size", flash_size);
    cJSON_AddNumberToObject(sys, "flash_speed", 0);
    cJSON_AddStringToObject(sys, "chip_model", "ESP32-P4");
    cJSON_AddNumberToObject(sys, "chip_revision", chip.revision);
    cJSON_AddNumberToObject(sys, "chip_cores", chip.cores);
    cJSON_AddNumberToObject(sys, "cpu_freq", 360);
    cJSON_AddStringToObject(sys, "sdk_version", esp_get_idf_version());
    float tc = web_hook_system_temperature_c();
    cJSON_AddNumberToObject(sys, "temperature_celsius", tc);
    cJSON_AddNumberToObject(sys, "temperature_fahrenheit", tc * 9.0f / 5.0f + 32.0f);
    cJSON_AddBoolToObject(sys, "healthy", web_hook_system_healthy());

    /* network */
    cJSON *nw = cJSON_AddObjectToObject(root, "network");
    cJSON_AddBoolToObject(nw, "connected", net.connected);
    if (net.connected) {
        cJSON_AddStringToObject(nw, "ssid", net.ssid);
        cJSON_AddStringToObject(nw, "ip", net.ip);
        cJSON_AddStringToObject(nw, "gateway", net.gateway);
        cJSON_AddStringToObject(nw, "dns", net.dns);
        cJSON_AddStringToObject(nw, "mac", net.mac);
        cJSON_AddNumberToObject(nw, "rssi", net.rssi);
        cJSON_AddStringToObject(nw, "bssid", net.bssid);
        cJSON_AddStringToObject(nw, "hostname", net.hostname);
    } else {
        cJSON_AddNullToObject(nw, "ssid");
        cJSON_AddNullToObject(nw, "ip");
        cJSON_AddNullToObject(nw, "gateway");
        cJSON_AddNullToObject(nw, "dns");
        cJSON_AddStringToObject(nw, "mac", net.mac);
        cJSON_AddNumberToObject(nw, "rssi", 0);
        cJSON_AddNullToObject(nw, "bssid");
        cJSON_AddNullToObject(nw, "hostname");
    }

    /* mqtt */
    cJSON *mq = cJSON_AddObjectToObject(root, "mqtt");
    cJSON_AddBoolToObject(mq, "connected", web_hook_mqtt_connected());
    app_config_get_mqtt_server(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(mq, "server", sbuf);
    cJSON_AddNumberToObject(mq, "port", app_config_get_mqtt_port());
    app_config_get_mqtt_client(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(mq, "client_id", sbuf);
    app_config_get_mqtt_user(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(mq, "username", sbuf);

    /* home_assistant */
    cJSON *ha = cJSON_AddObjectToObject(root, "home_assistant");
    cJSON_AddBoolToObject(ha, "discovery_enabled", app_config_get_ha_disc_en());
    app_config_get_ha_dev_name(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(ha, "device_name", sbuf);
    app_config_get_ha_disc_pfx(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(ha, "discovery_prefix", sbuf);
    app_config_get_ha_state_top(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(ha, "state_topic", sbuf);
    cJSON_AddNumberToObject(ha, "sensor_update_interval", app_config_get_ha_sens_int());

    /* ha_rest / light-sensor auto-brightness (token is sensitive: emit a flag) */
    cJSON *hr = cJSON_AddObjectToObject(root, "ha_rest");
    app_config_get_ha_base_url(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(hr, "ha_base_url", sbuf);
    cJSON_AddNumberToObject(hr, "ha_poll_interval", app_config_get_ha_poll_int());
    app_config_get_ha_token(sbuf, sizeof(sbuf));
    cJSON_AddBoolToObject(hr, "ha_token_set", sbuf[0] != '\0');
    app_config_get_ha_sensor_ent(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(hr, "ha_light_sensor_entity", sbuf);
    cJSON_AddNumberToObject(hr, "light_sensor_mapping_mode", app_config_get_sensor_map_mode());
    cJSON_AddNumberToObject(hr, "light_sensor_min_lux", app_config_get_sensor_min_lux());
    cJSON_AddNumberToObject(hr, "light_sensor_max_lux", app_config_get_sensor_max_lux());
    cJSON_AddNumberToObject(hr, "display_min_brightness", app_config_get_disp_min_br());
    cJSON_AddNumberToObject(hr, "display_max_brightness", app_config_get_disp_max_br());

    /* logging */
    cJSON *lg = cJSON_AddObjectToObject(root, "logging");
    cJSON_AddNumberToObject(lg, "min", app_config_get_log_min_sev());

    /* display */
    cJSON *disp = cJSON_AddObjectToObject(root, "display");
    cJSON_AddNumberToObject(disp, "width", panel.width);
    cJSON_AddNumberToObject(disp, "height", panel.height);
    cJSON_AddNumberToObject(disp, "brightness", current_brightness());
    cJSON_AddBoolToObject(disp, "brightness_auto_mode", app_config_get_bright_auto());
    cJSON_AddBoolToObject(disp, "use_ha_rest_control", app_config_get_use_ha_rest());
    cJSON_AddNumberToObject(disp, "backlight_freq", app_config_get_bl_freq());
    cJSON_AddNumberToObject(disp, "backlight_resolution", app_config_get_bl_res());
    cJSON_AddNumberToObject(disp, "color_temp", app_config_get_color_temp());

    /* image */
    cJSON *img = cJSON_AddObjectToObject(root, "image");
    bool cycling = app_config_get_cycling_en();
    cJSON_AddBoolToObject(img, "cycling_enabled", cycling);
    cJSON_AddNumberToObject(img, "update_interval", app_config_get_upd_interval());
    app_config_current_image_url(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(img, "current_url", sbuf);

    /* Source list, count and cycling defaults are always present so the UI can
     * render them regardless of whether cycling is currently enabled. */
    cJSON_AddNumberToObject(img, "default_image_duration", app_config_get_def_img_dur());
    cJSON_AddBoolToObject(img, "random_order", app_config_get_random_ord());
    cJSON_AddNumberToObject(img, "current_index", app_config_get_curr_img_idx());
    int cnt = app_config_source_count();
    cJSON_AddNumberToObject(img, "source_count", cnt);
    cJSON *arr = cJSON_AddArrayToObject(img, "sources");
    int cur = web_hook_image_current_index();
    for (int i = 0; i < cnt; i++) {
        image_source_t s;
        if (!app_config_get_source(i, &s)) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "index", i);
        cJSON_AddStringToObject(o, "url", s.url);
        cJSON_AddBoolToObject(o, "enabled", s.enabled);
        cJSON_AddBoolToObject(o, "active", i == cur);
        cJSON_AddNumberToObject(o, "scale_x", s.transform.scale_x);
        cJSON_AddNumberToObject(o, "scale_y", s.transform.scale_y);
        cJSON_AddNumberToObject(o, "offset_x", s.transform.offset_x);
        cJSON_AddNumberToObject(o, "offset_y", s.transform.offset_y);
        cJSON_AddNumberToObject(o, "rotation", s.transform.rotation);
        cJSON_AddItemToArray(arr, o);
    }

    if (cycling) {
        cJSON_AddNumberToObject(img, "cycle_interval", app_config_get_cycle_intv());
    } else {
        app_config_get_image_url(sbuf, sizeof(sbuf));
        cJSON_AddStringToObject(img, "url", sbuf);
    }

    /* defaults */
    cJSON *def = cJSON_AddObjectToObject(root, "defaults");
    cJSON_AddNumberToObject(def, "brightness", app_config_get_def_bright());
    cJSON_AddNumberToObject(def, "scale_x", app_config_get_def_scale_x());
    cJSON_AddNumberToObject(def, "scale_y", app_config_get_def_scale_y());
    cJSON_AddNumberToObject(def, "offset_x", app_config_get_def_off_x());
    cJSON_AddNumberToObject(def, "offset_y", app_config_get_def_off_y());
    cJSON_AddNumberToObject(def, "rotation", app_config_get_def_rot());

    /* advanced */
    cJSON *adv = cJSON_AddObjectToObject(root, "advanced");
    cJSON_AddNumberToObject(adv, "mqtt_reconnect_interval", app_config_get_mqtt_recon());
    cJSON_AddNumberToObject(adv, "watchdog_timeout", app_config_get_wd_timeout());
    cJSON_AddNumberToObject(adv, "critical_heap_threshold", app_config_get_heap_thresh());
    cJSON_AddNumberToObject(adv, "critical_psram_threshold", app_config_get_psram_thresh());
    cJSON_AddNumberToObject(adv, "display_type", app_config_get_disp_type());

    /* time */
    cJSON *tm = cJSON_AddObjectToObject(root, "time");
    cJSON_AddBoolToObject(tm, "ntp_enabled", app_config_get_ntp_enabled());
    app_config_get_ntp_server(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(tm, "ntp_server", sbuf);
    app_config_get_timezone(sbuf, sizeof(sbuf));
    cJSON_AddStringToObject(tm, "timezone", sbuf);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { httpd_resp_send_500(req); return ESP_FAIL; }
    esp_err_t r = web_send_json(req, 200, out);
    free(out);
    return r;
}

/* ---- GET /api/health ----------------------------------------------------- */
esp_err_t web_health_handler(httpd_req_t *req)
{
    char *buf = malloc(4096);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    web_hook_health_report_json(buf, 4096);
    esp_err_t r = web_send_json(req, 200, buf);
    free(buf);
    return r;
}

/* ---- GET /api/wifi-scan -------------------------------------------------- */
esp_err_t web_wifi_scan_handler(httpd_req_t *req)
{
    char *buf = malloc(4096);
    if (!buf) { httpd_resp_send_500(req); return ESP_FAIL; }
    buf[0] = '\0';
    int n = web_hook_net_wifi_scan(buf, 4096);
    esp_err_t r;
    if (n == -2) {
        r = web_send_status(req, 429, "error", "Scan rate limited - please wait 10 seconds between scans", NULL);
    } else if (n < 0) {
        r = web_send_status(req, 500, "error", "WiFi scan failed", NULL);
    } else {
        char *out = malloc(4096 + 64);
        if (!out) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
        snprintf(out, 4096 + 64, "{\"status\":\"success\",\"networks\":%s}",
                 buf[0] ? buf : "[]");
        r = web_send_json(req, 200, out);
        free(out);
    }
    free(buf);
    return r;
}

/* ---- GET /api/current-image ---------------------------------------------- */
esp_err_t web_current_image_handler(httpd_req_t *req)
{
    char url[256]; app_config_current_image_url(url, sizeof(url));
    char url_esc[300] = ""; web_json_escape(url_esc, sizeof(url_esc), url);
    bool cycling = app_config_get_cycling_en();

    char body[640];
    if (cycling) {
        snprintf(body, sizeof(body),
            "{\"status\":\"success\",\"current_url\":\"%s\",\"cycling_enabled\":true,"
            "\"current_index\":%d,\"total_sources\":%d,"
            "\"message\":\"Image data is displayed on the device. Use the current URL to fetch the source image.\"}",
            url_esc, (int)app_config_get_curr_img_idx(), app_config_source_count());
    } else {
        snprintf(body, sizeof(body),
            "{\"status\":\"success\",\"current_url\":\"%s\",\"cycling_enabled\":false,"
            "\"message\":\"Image data is displayed on the device. Use the current URL to fetch the source image.\"}",
            url_esc);
    }
    return web_send_json(req, 200, body);
}

/* ---- moon settings object shared with getMoon + images/state ------------- */
static void moon_json(cJSON *m)
{
    cJSON_AddNumberToObject(m, "lat", app_config_get_moonLat());
    cJSON_AddNumberToObject(m, "lon", app_config_get_moonLon());
    cJSON_AddNumberToObject(m, "bg", app_config_get_moonBg());
    cJSON_AddNumberToObject(m, "flipu", app_config_get_mFlipU());
    cJSON_AddNumberToObject(m, "flipv", app_config_get_mFlipV());
    cJSON_AddNumberToObject(m, "roll", app_config_get_mRollOff());
    cJSON_AddNumberToObject(m, "yaw", app_config_get_mYawOff());
    cJSON_AddNumberToObject(m, "pitch", app_config_get_mPitchOff());
    cJSON_AddNumberToObject(m, "northup", app_config_get_mNorthUp());
    cJSON_AddNumberToObject(m, "light", app_config_get_mDragLM());
    cJSON_AddNumberToObject(m, "spin", app_config_get_mSpinMode());
    cJSON_AddNumberToObject(m, "spinret", app_config_get_mSpinRet());
}

/* ---- GET /api/getMoon ---------------------------------------------------- */
esp_err_t web_get_moon_handler(httpd_req_t *req)
{
    cJSON *m = cJSON_CreateObject();
    if (!m) { httpd_resp_send_500(req); return ESP_FAIL; }
    moon_json(m);
    char *out = cJSON_PrintUnformatted(m);
    cJSON_Delete(m);
    if (!out) { httpd_resp_send_500(req); return ESP_FAIL; }
    esp_err_t r = web_send_json(req, 200, out);
    free(out);
    return r;
}

/* ---- GET /api/images/state ----------------------------------------------- */
esp_err_t web_images_state_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) { httpd_resp_send_500(req); return ESP_FAIL; }

    cJSON_AddNumberToObject(root, "currentIndex", web_hook_image_current_index());

    cJSON *tuning = cJSON_AddObjectToObject(root, "tuning");
    int tune_idx = web_editing_tune_index();
    cJSON_AddBoolToObject(tuning, "active", tune_idx >= 0);
    cJSON_AddNumberToObject(tuning, "index", tune_idx);

    cJSON_AddNumberToObject(root, "maxScale", WEB_MAX_SCALE);
    cJSON_AddNumberToObject(root, "updateMode", app_config_get_img_upd_mode());
    cJSON_AddNumberToObject(root, "defaultDuration", app_config_get_def_img_dur());
    cJSON_AddNumberToObject(root, "updateInterval", app_config_get_upd_interval() / 60000);
    cJSON_AddBoolToObject(root, "randomOrder", app_config_get_random_ord());

    cJSON *def = cJSON_AddObjectToObject(root, "defaults");
    cJSON_AddNumberToObject(def, "scaleX", app_config_get_def_scale_x());
    cJSON_AddNumberToObject(def, "scaleY", app_config_get_def_scale_y());
    cJSON_AddNumberToObject(def, "offsetX", app_config_get_def_off_x());
    cJSON_AddNumberToObject(def, "offsetY", app_config_get_def_off_y());
    cJSON_AddNumberToObject(def, "rotation", (int)app_config_get_def_rot());

    cJSON *moon = cJSON_AddObjectToObject(root, "moon");
    moon_json(moon);

    cJSON *presets = cJSON_AddArrayToObject(root, "presets");
    for (int i = 0; i < g_web_preset_count; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", g_web_presets[i].id);
        cJSON_AddStringToObject(p, "label", g_web_presets[i].label);
        cJSON_AddItemToArray(presets, p);
    }
    cJSON *pm = cJSON_CreateObject();
    cJSON_AddStringToObject(pm, "id", "__moon__");
    cJSON_AddStringToObject(pm, "label", "Moon (computed)");
    cJSON_AddItemToArray(presets, pm);

    cJSON *arr = cJSON_AddArrayToObject(root, "sources");
    int cnt = app_config_source_count();
    for (int i = 0; i < cnt; i++) {
        image_source_t s;
        if (!app_config_get_source(i, &s)) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "index", i);
        cJSON_AddStringToObject(o, "url", s.url);
        cJSON_AddBoolToObject(o, "enabled", s.enabled);
        cJSON_AddNumberToObject(o, "duration", s.duration);
        cJSON_AddNumberToObject(o, "scaleX", s.transform.scale_x);
        cJSON_AddNumberToObject(o, "scaleY", s.transform.scale_y);
        cJSON_AddNumberToObject(o, "offsetX", s.transform.offset_x);
        cJSON_AddNumberToObject(o, "offsetY", s.transform.offset_y);
        cJSON_AddNumberToObject(o, "rotation", (int)s.transform.rotation);
        cJSON_AddBoolToObject(o, "isMoon", strncmp(s.url, "moon://", 7) == 0);
        cJSON_AddItemToArray(arr, o);
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { httpd_resp_send_500(req); return ESP_FAIL; }
    esp_err_t r = web_send_json(req, 200, out);
    free(out);
    (void)TAG;
    return r;
}
