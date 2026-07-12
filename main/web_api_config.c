#include "web_internal.h"
#include "config_backup.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

static const char *TAG = "web_api_config";

/* ---- delayed reboot ------------------------------------------------------ */

static void reboot_after_flush(void)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    web_hook_crash_save();
    esp_restart();
}

/* ---- POST /api/save ------------------------------------------------------ */

/* Checkbox with hidden companion `<field>_present=1`. Returns true if the
 * request addresses this checkbox, writing the intended boolean into *val. */
static bool checkbox_addressed(const char *body, const char *field, bool *val)
{
    char marker[48];
    snprintf(marker, sizeof(marker), "%s_present", field);
    char v[8];
    /* Frontend always POSTs the checkbox key (empty value when unchecked), so
     * presence alone is not enough: a non-empty value means checked. */
    bool has_field  = web_form_get(body, field, v, sizeof(v)) && v[0] != '\0';
    bool has_marker = web_form_has(body, marker);
    if (!has_field && !has_marker) return false;
    *val = has_field;
    return true;
}

static bool get_str(const char *body, const char *key, char *out, size_t len)
{
    return web_form_get(body, key, out, len);
}

esp_err_t web_save_handler(httpd_req_t *req)
{
    char *body = malloc(4096);
    if (!body) { httpd_resp_send_500(req); return ESP_FAIL; }
    if (web_read_body(req, body, 4096) < 0) { free(body); return web_send_400(req, "Invalid request body"); }

    char buf[300];
    bool needs_restart = false;
    bool brightness_applied = false;
    bool image_applied = false;
    bool ntp_touched = false;

    /* -- strings / scalars -- */
    if (get_str(body, "device_name", buf, sizeof(buf))) app_config_set_device_name(buf);

    if (get_str(body, "wifi_ssid", buf, sizeof(buf))) { app_config_set_wifi_ssid(buf); needs_restart = true; }
    if (get_str(body, "wifi_password", buf, sizeof(buf)) && buf[0]) { app_config_set_wifi_pwd(buf); needs_restart = true; }

    if (get_str(body, "mqtt_server", buf, sizeof(buf))) { app_config_set_mqtt_server(buf); needs_restart = true; }
    if (get_str(body, "mqtt_port", buf, sizeof(buf)) && buf[0]) { app_config_set_mqtt_port(atoi(buf)); needs_restart = true; }
    if (get_str(body, "mqtt_user", buf, sizeof(buf))) app_config_set_mqtt_user(buf);
    if (get_str(body, "mqtt_password", buf, sizeof(buf)) && buf[0]) app_config_set_mqtt_pwd(buf);
    if (get_str(body, "mqtt_client_id", buf, sizeof(buf))) app_config_set_mqtt_client(buf);

    if (get_str(body, "ha_device_name", buf, sizeof(buf))) app_config_set_ha_dev_name(buf);
    if (get_str(body, "ha_discovery_prefix", buf, sizeof(buf))) app_config_set_ha_disc_pfx(buf);
    if (get_str(body, "ha_state_topic", buf, sizeof(buf))) app_config_set_ha_state_top(buf);
    if (get_str(body, "ha_sensor_update_interval", buf, sizeof(buf)) && buf[0]) app_config_set_ha_sens_int((uint32_t)atoi(buf));

    if (get_str(body, "image_url", buf, sizeof(buf))) { app_config_set_image_url(buf); image_applied = true; }

    if (get_str(body, "default_brightness", buf, sizeof(buf)) && buf[0]) {
        int b = atoi(buf);
        app_config_set_def_bright(b);
        web_hook_apply_brightness(b);
        brightness_applied = true;
    }
    if (get_str(body, "default_scale_x", buf, sizeof(buf)) && buf[0]) { app_config_set_def_scale_x(strtof(buf, NULL)); image_applied = true; }
    if (get_str(body, "default_scale_y", buf, sizeof(buf)) && buf[0]) { app_config_set_def_scale_y(strtof(buf, NULL)); image_applied = true; }
    if (get_str(body, "default_offset_x", buf, sizeof(buf)) && buf[0]) { app_config_set_def_off_x(atoi(buf)); image_applied = true; }
    if (get_str(body, "default_offset_y", buf, sizeof(buf)) && buf[0]) { app_config_set_def_off_y(atoi(buf)); image_applied = true; }
    if (get_str(body, "default_rotation", buf, sizeof(buf)) && buf[0]) { app_config_set_def_rot(strtof(buf, NULL)); image_applied = true; }
    if (get_str(body, "color_temp", buf, sizeof(buf)) && buf[0]) { app_config_set_color_temp(atoi(buf)); image_applied = true; }

    if (get_str(body, "backlight_freq", buf, sizeof(buf)) && buf[0]) app_config_set_bl_freq(atoi(buf));
    if (get_str(body, "backlight_resolution", buf, sizeof(buf)) && buf[0]) app_config_set_bl_res(atoi(buf));

    if (get_str(body, "cycle_interval", buf, sizeof(buf)) && buf[0]) app_config_set_cycle_intv((uint32_t)atol(buf) * 1000u);
    if (get_str(body, "image_update_mode", buf, sizeof(buf)) && buf[0]) app_config_set_img_upd_mode(atoi(buf));
    if (get_str(body, "default_image_duration", buf, sizeof(buf)) && buf[0]) app_config_set_def_img_dur((uint32_t)atol(buf));
    if (get_str(body, "update_interval", buf, sizeof(buf)) && buf[0]) app_config_set_upd_interval((uint32_t)atol(buf) * 60000u);
    if (get_str(body, "mqtt_reconnect_interval", buf, sizeof(buf)) && buf[0]) app_config_set_mqtt_recon((uint32_t)atol(buf) * 1000u);
    if (get_str(body, "watchdog_timeout", buf, sizeof(buf)) && buf[0]) app_config_set_wd_timeout((uint32_t)atol(buf) * 1000u);
    if (get_str(body, "critical_heap_threshold", buf, sizeof(buf)) && buf[0]) app_config_set_heap_thresh((uint32_t)atol(buf));
    if (get_str(body, "critical_psram_threshold", buf, sizeof(buf)) && buf[0]) app_config_set_psram_thresh((uint32_t)atol(buf));

    if (get_str(body, "display_type", buf, sizeof(buf)) && buf[0]) { app_config_set_disp_type(atoi(buf)); needs_restart = true; }

    if (get_str(body, "ha_base_url", buf, sizeof(buf))) app_config_set_ha_base_url(buf);
    if (get_str(body, "ha_access_token", buf, sizeof(buf)) && buf[0]) app_config_set_ha_token(buf);
    if (get_str(body, "ha_light_sensor_entity", buf, sizeof(buf))) app_config_set_ha_sensor_ent(buf);
    if (get_str(body, "light_sensor_min_lux", buf, sizeof(buf)) && buf[0]) app_config_set_sensor_min_lux(strtof(buf, NULL));
    if (get_str(body, "light_sensor_max_lux", buf, sizeof(buf)) && buf[0]) app_config_set_sensor_max_lux(strtof(buf, NULL));
    if (get_str(body, "display_min_brightness", buf, sizeof(buf)) && buf[0]) app_config_set_disp_min_br(atoi(buf));
    if (get_str(body, "display_max_brightness", buf, sizeof(buf)) && buf[0]) app_config_set_disp_max_br(atoi(buf));
    if (get_str(body, "ha_poll_interval", buf, sizeof(buf)) && buf[0]) app_config_set_ha_poll_int((uint32_t)atol(buf));
    if (get_str(body, "light_sensor_mapping_mode", buf, sizeof(buf)) && buf[0]) app_config_set_sensor_map_mode(atoi(buf));

    if (get_str(body, "ntp_server", buf, sizeof(buf))) { app_config_set_ntp_server(buf); ntp_touched = true; }
    if (get_str(body, "timezone", buf, sizeof(buf))) { app_config_set_timezone(buf); ntp_touched = true; }

    /* -- checkboxes -- */
    bool cb;
    if (checkbox_addressed(body, "ha_discovery_enabled", &cb)) app_config_set_ha_disc_en(cb);
    if (checkbox_addressed(body, "ntp_enabled", &cb)) { app_config_set_ntp_enabled(cb); ntp_touched = true; }
    if (checkbox_addressed(body, "random_order", &cb)) app_config_set_random_ord(cb);

    bool cycling_before = app_config_get_cycling_en();
    bool cycling_changed = false;
    if (checkbox_addressed(body, "cycling_enabled", &cb)) {
        if (cb != cycling_before) cycling_changed = true;
        app_config_set_cycling_en(cb);
    }

    /* brightness_auto_mode then use_ha_rest_control (HA processed last, wins). */
    if (checkbox_addressed(body, "brightness_auto_mode", &cb)) {
        app_config_set_bright_auto(cb);
        if (cb) app_config_set_use_ha_rest(false);
    }
    if (checkbox_addressed(body, "use_ha_rest_control", &cb)) {
        app_config_set_use_ha_rest(cb);
        if (cb) app_config_set_bright_auto(false);
    }

    app_config_save();

    /* -- side effects -- */
    if (image_applied) web_hook_image_rerender();
    if (ntp_touched) web_hook_net_resync_time();
    if (cycling_changed) {
        if (app_config_get_cycling_en()) app_config_set_curr_img_idx(0);
        app_config_save();
        web_hook_image_cycling_changed(app_config_get_cycling_en());
    }

    free(body);

    char msg[160];
    int n = snprintf(msg, sizeof(msg), "Configuration saved");
    if (needs_restart) n += snprintf(msg + n, sizeof(msg) - n, " (restart required to apply)");
    if (brightness_applied) n += snprintf(msg + n, sizeof(msg) - n, " - brightness applied immediately");
    if (image_applied) n += snprintf(msg + n, sizeof(msg) - n, " - image settings applied immediately");

    char extra[48];
    snprintf(extra, sizeof(extra), "\"needsRestart\":%s", needs_restart ? "true" : "false");
    return web_send_status(req, 200, "success", msg, extra);
}

/* ---- GET /api/backup ----------------------------------------------------- */

esp_err_t web_backup_handler(httpd_req_t *req)
{
    char secrets[8] = "";
    bool include = false;
    if (web_query_get(req, "secrets", secrets, sizeof(secrets))) include = (secrets[0] == '1');

    char *json = NULL;
    if (config_backup_export(&json, include) != ESP_OK || !json) {
        if (json) free(json);
        return web_send_status(req, 500, "error", "Backup export failed", NULL);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"allsky-config.json\"");
    esp_err_t r = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return r;
}

/* ---- POST /api/restore --------------------------------------------------- */

esp_err_t web_restore_handler(httpd_req_t *req)
{
    if (req->content_len == 0) return web_send_400(req, "Request body is empty. Upload a backup file.");
    size_t len = req->content_len;
    if (len > 32768) return web_send_400(req, "Backup file too large");
    char *body = malloc(len + 1);
    if (!body) { httpd_resp_send_500(req); return ESP_FAIL; }
    if (web_read_body(req, body, len + 1) < 0) { free(body); return web_send_400(req, "Failed to read request body"); }

    config_restore_result_t res;
    memset(&res, 0, sizeof(res));
    config_backup_restore(body, 0, &res);
    free(body);

    char extra[128];
    snprintf(extra, sizeof(extra), "\"applied\":%d,\"skipped\":%d,\"fileVersion\":%d,\"versionMismatch\":%s",
             res.applied, res.skipped, res.file_version, res.version_mismatch ? "true" : "false");

    if (!res.ok) return web_send_status(req, 400, "error", res.message, extra);

    esp_err_t r = web_send_status(req, 200, "success", res.message[0] ? res.message : "Configuration restored. Device restarting now...", extra);
    reboot_after_flush();  /* does not return */
    return r;
}

/* ---- POST /api/restart --------------------------------------------------- */

esp_err_t web_restart_handler(httpd_req_t *req)
{
    web_hook_display_message("Restarting...");
    esp_err_t r = web_send_ok(req, "Device restarting now...");
    reboot_after_flush();
    return r;
}

/* ---- POST /api/factory-reset --------------------------------------------- */

esp_err_t web_factory_reset_handler(httpd_req_t *req)
{
    app_config_factory_reset();
    web_hook_display_message("Factory reset");
    esp_err_t r = web_send_ok(req, "Factory reset completed. WiFi setup will run on next boot. Device restarting...");
    reboot_after_flush();
    return r;
}

/* ---- POST /api/set-log-severity ------------------------------------------ */

esp_err_t web_set_log_severity_handler(httpd_req_t *req)
{
    char body[64];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    char sv[8];
    if (!web_form_get(body, "severity", sv, sizeof(sv)) || sv[0] == '\0')
        return web_send_400(req, "Missing required parameter: severity");
    int sev = atoi(sv);
    if (sev < 0 || sev > 4) return web_send_400(req, "Severity must be 0-4");

    app_config_set_log_min_sev(sev);
    app_config_save();
    web_ws_set_min_severity(sev);

    static const char *names[] = { "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL" };
    char msg[64], extra[24];
    snprintf(msg, sizeof(msg), "Log severity filter updated to %s", names[sev]);
    snprintf(extra, sizeof(extra), "\"severity\":%d", sev);
    return web_send_status(req, 200, "success", msg, extra);
}

/* ---- POST /api/clear-crash-logs ------------------------------------------ */

esp_err_t web_clear_crash_logs_handler(httpd_req_t *req)
{
    web_hook_crash_clear();
    return web_send_ok(req, "Crash logs cleared from RTC and NVS storage");
}

/* ---- POST /api/force-brightness-update ----------------------------------- */

esp_err_t web_force_brightness_handler(httpd_req_t *req)
{
    if (web_hook_ha_rest_reapply_brightness()) {
        /* HA REST applied a fresh reading. */
    } else if (app_config_get_bright_auto()) {
        web_hook_mqtt_publish_state();
    } else {
        web_hook_apply_brightness(app_config_get_def_bright());
    }
    (void)TAG;
    return web_send_ok(req, "Brightness updated");
}
