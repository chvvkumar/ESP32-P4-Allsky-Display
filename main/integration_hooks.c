/*
 * Strong definitions of the remaining web-integration hooks (image pipeline,
 * network, MQTT, HA REST, brightness) plus the cross-subsystem adapters app_main
 * installs into subsystem hook structs. These override the WEAK stubs in
 * web_integration_stubs.c. System-core hooks (health, temperature, crash/boot
 * log, log stream) are owned by system_web_bridge.c and are NOT redefined here.
 * Captive-portal hooks remain the weak no-ops: provisioning is served by the
 * network manager's own portal server (network_portal), not the web captive TU.
 */

#include "integration_hooks.h"
#include "web_integration.h"

#include "brightness_ctrl.h"
#include "image_pipeline.h"
#include "image_ppa.h"
#include "image_sink.h"
#include "network_wifi.h"
#include "network_time.h"
#include "mqtt_ha.h"
#include "web_server.h"
#include "app_config.h"
#include "display_defs.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "integ";

/* ---- Display / rendering ------------------------------------------------- */

void web_hook_apply_brightness(int percent)
{
    brightness_apply(percent);
}

/* ---- Image pipeline ------------------------------------------------------ */

int web_hook_image_current_index(void)
{
    return (int)app_config_get_curr_img_idx();
}

void web_hook_image_queue_download(void)
{
    image_pipeline_request_refresh();
}

void web_hook_image_next(void)
{
    image_pipeline_next_image();
}

void web_hook_image_rerender(void)
{
    image_pipeline_notify_edit();
}

void web_hook_image_set_editing_paused(bool paused)
{
    image_pipeline_set_tune_pause(paused);
}

void web_hook_image_tune(int index, bool persist_index)
{
    (void)persist_index;
    /* Hold the requested source on screen: point the current index at it, pause
     * cycling for tuning, and force an immediate (re)render. The pipeline has no
     * runtime-only index override, so the index is set through config (debounced
     * persistence) in both persist and preview cases. */
    if (index >= 0 && index < app_config_source_count()) {
        app_config_set_curr_img_idx((int32_t)index);
    }
    image_pipeline_set_tune_pause(true);
    image_pipeline_request_refresh();
}

void web_hook_image_tune_stop(void)
{
    image_pipeline_set_tune_pause(false);
}

void web_hook_image_cycling_changed(bool cycling_enabled)
{
    (void)cycling_enabled;
    image_pipeline_request_refresh();
}

/* ---- Network ------------------------------------------------------------- */

void web_hook_net_status(web_net_status_t *out)
{
    if (!out) {
        return;
    }
    network_wifi_status_t s;
    network_wifi_get_status(&s);

    memset(out, 0, sizeof(*out));
    out->connected = s.connected;
    out->rssi = s.rssi;
    strlcpy(out->ssid, s.ssid, sizeof(out->ssid));
    strlcpy(out->ip, s.ip, sizeof(out->ip));
    strlcpy(out->gateway, s.gateway, sizeof(out->gateway));
    strlcpy(out->dns, s.dns, sizeof(out->dns));
    strlcpy(out->mac, s.mac, sizeof(out->mac));
    strlcpy(out->bssid, s.bssid, sizeof(out->bssid));
    strlcpy(out->hostname, s.hostname, sizeof(out->hostname));
}

int web_hook_net_wifi_scan(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return -1;
    }
    buf[0] = '\0';

    char *tmp = malloc(4096);
    if (!tmp) {
        return -1;
    }
    tmp[0] = '\0';

    int n = network_wifi_scan_json(tmp, 4096);
    if (n < 0) {
        free(tmp);
        return n;   /* propagate -1 (failure) / -2 (rate limited) */
    }

    /* network_wifi_scan_json emits {"status":..,"networks":[..],"count":N}; the
     * web endpoint wants the bare array. Slice from the '[' after "networks":
     * up to (and including) the ']' just before ,"count": . */
    const char *nk = strstr(tmp, "\"networks\":");
    const char *start = nk ? strchr(nk, '[') : NULL;
    const char *cnt = strstr(tmp, ",\"count\":");
    if (start && cnt && cnt > start) {
        size_t len = (size_t)(cnt - start);
        if (len >= buf_len) {
            len = buf_len - 1;
        }
        memcpy(buf, start, len);
        buf[len] = '\0';
    } else {
        strlcpy(buf, "[]", buf_len);
    }
    free(tmp);
    return n;
}

void web_hook_net_resync_time(void)
{
    network_time_resync();
}

/* ---- MQTT ---------------------------------------------------------------- */

bool web_hook_mqtt_connected(void)
{
    return mqtt_ha_is_connected();
}

void web_hook_mqtt_publish_state(void)
{
    mqtt_ha_publish_state();
}

void web_hook_mqtt_sources_changed(void)
{
    mqtt_ha_notify_sources_changed();
}

/* ---- HA REST brightness -------------------------------------------------- */

bool web_hook_ha_rest_reapply_brightness(void)
{
    if (!mqtt_ha_rest_last(NULL, NULL)) {
        return false;   /* HA REST not active / no successful poll yet */
    }
    brightness_reapply_active_mode();
    return true;
}

/* ---- Cross-subsystem adapters (installed by app_main) -------------------- */

int integ_wifi_rssi(void)
{
    network_wifi_status_t s;
    network_wifi_get_status(&s);
    return s.rssi;
}

void integ_wifi_bssid(char *out, size_t len)
{
    if (!out || len == 0) {
        return;
    }
    network_wifi_status_t s;
    network_wifi_get_status(&s);
    strlcpy(out, s.bssid, len);
}

bool integ_mqtt_rssi(int8_t *out)
{
    if (!out) {
        return false;
    }
    network_wifi_status_t s;
    network_wifi_get_status(&s);
    if (!s.connected) {
        return false;
    }
    *out = (int8_t)s.rssi;
    return true;
}

void integ_image_index_changed(int new_index)
{
    (void)new_index;   /* config already holds the new index; just re-fetch. */
    image_pipeline_request_refresh();
}

void integ_restart_web_server(void)
{
    web_server_stop();
    web_server_start();
}

void integ_single_tap(void)
{
    if (!image_pipeline_current_is_moon()) {
        image_pipeline_next_image();
    }
}

/* ---- Serial diagnostic report providers ---------------------------------- */

void integ_serial_web_status(char *out, size_t len)
{
    if (!out || len == 0) {
        return;
    }
    bool running = web_server_is_running();
    bool auth = web_auth_is_enabled();
    snprintf(out, len, "Web server: %s (port %d)\nAuth: %s",
             running ? "running" : "stopped", 8080,
             auth ? "enabled" : "disabled");
}

void integ_serial_network_info(char *out, size_t len)
{
    if (!out || len == 0) {
        return;
    }
    network_wifi_status_t s;
    network_wifi_get_status(&s);
    snprintf(out, len,
             "Status:   %s\n"
             "Hostname: %s\n"
             "SSID:     %s\n"
             "IP:       %s\n"
             "Gateway:  %s\n"
             "RSSI:     %d dBm\n"
             "Clock:    %s",
             s.connected ? "connected" : "disconnected",
             s.hostname, s.ssid[0] ? s.ssid : "-", s.ip, s.gateway, s.rssi,
             network_time_is_valid() ? "valid" : "not synced");
}

void integ_serial_ppa_info(char *out, size_t len)
{
    if (!out || len == 0) {
        return;
    }
    /* No runtime PPA counter is exposed; report the fixed engine configuration
     * plus the pipeline state that drives it. */
    snprintf(out, len,
             "PPA engine: RGB565 SRM hardware scaler (single shared client)\n"
             "First image: %s\n"
             "Current source: %s",
             image_pipeline_has_first_image() ? "shown" : "pending",
             image_pipeline_current_is_moon() ? "moon:// (local render)" : "downloaded");
}

/* ---- Moon interactive drag sinks ----------------------------------------- */

/* Two lazily-allocated panel-sized scratch buffers, used ping-pong. LVGL reads
 * the previously presented buffer asynchronously, so the frame being scaled this
 * call must not be the one handed to the sink last call. */
static uint16_t *s_moon_blit_dst[2];
static size_t    s_moon_blit_cap;
static int       s_moon_blit_idx;

void integ_moon_blit(const uint16_t *src, int src_w, int src_h)
{
    if (!src || src_w <= 0 || src_h <= 0) {
        return;
    }
    allsky_panel_profile_t panel = allsky_panel_profile();
    int pw = panel.width;
    int ph = panel.height;

    if (!s_moon_blit_dst[0]) {
        s_moon_blit_cap = (size_t)pw * ph * sizeof(uint16_t);
        for (int i = 0; i < 2; i++) {
            s_moon_blit_dst[i] = heap_caps_aligned_alloc(128, s_moon_blit_cap, MALLOC_CAP_SPIRAM);
            if (!s_moon_blit_dst[i]) {
                ESP_LOGW(TAG, "moon blit buffer alloc failed (%u bytes)", (unsigned)s_moon_blit_cap);
                for (int j = 0; j < 2; j++) {
                    free(s_moon_blit_dst[j]);
                    s_moon_blit_dst[j] = NULL;
                }
                s_moon_blit_cap = 0;
                return;
            }
        }
    }

    uint16_t *dst = s_moon_blit_dst[s_moon_blit_idx];
    s_moon_blit_idx ^= 1;

    if (image_ppa_scale(src, (uint32_t)src_w, (uint32_t)src_h, 0,
                        dst, s_moon_blit_cap,
                        (uint32_t)pw, (uint32_t)ph) != ESP_OK) {
        return;   /* keep the previous frame on PPA failure */
    }

    if (bsp_display_lock(0)) {
        image_sink_present(dst, pw, ph);
        bsp_display_unlock();
    }
}

void integ_moon_settle(void)
{
    /* Force a crisp full-resolution resting re-render of the moon source. */
    image_pipeline_request_refresh();
}
