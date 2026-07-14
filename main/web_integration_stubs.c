/*
 * Weak default implementations of the web-server integration hooks. Each is a
 * safe no-op or neutral value so the web subsystem builds and runs before the
 * integration phase wires the real subsystems. A strong definition elsewhere in
 * the link overrides any of these.
 */

#include "web_integration.h"
#include <string.h>
#include <math.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"

#define WEAK __attribute__((weak))

WEAK void web_hook_apply_brightness(int percent) { (void)percent; }
WEAK void web_hook_display_pause(bool pause) { (void)pause; }
WEAK void web_hook_display_message(const char *msg) { (void)msg; }
WEAK void web_hook_ota_screen(int progress, const char *state_text) { (void)progress; (void)state_text; }

WEAK int  web_hook_image_current_index(void) { return 0; }
WEAK void web_hook_image_queue_download(void) {}
WEAK void web_hook_image_next(void) {}
WEAK void web_hook_image_rerender(void) {}
WEAK void web_hook_image_set_editing_paused(bool paused) { (void)paused; }
WEAK void web_hook_image_tune(int index, bool persist_index) { (void)index; (void)persist_index; }
WEAK void web_hook_image_tune_stop(void) {}
WEAK void web_hook_image_cycling_changed(bool cycling_enabled) { (void)cycling_enabled; }

WEAK void web_hook_net_status(web_net_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    strcpy(out->ip, "0.0.0.0");
    strcpy(out->gateway, "0.0.0.0");
    strcpy(out->dns, "0.0.0.0");
    strcpy(out->mac, "00:00:00:00:00:00");
    strcpy(out->bssid, "00:00:00:00:00:00");
}

WEAK int web_hook_net_wifi_scan(char *buf, size_t buf_len)
{
    if (buf && buf_len) buf[0] = '\0';
    return -1;
}

WEAK void web_hook_net_resync_time(void) {}

WEAK void web_hook_captive_scan_start(void) {}
WEAK size_t web_hook_captive_scan_results(char *buf, size_t buf_len)
{
    static const char *s = "{\"status\":\"complete\",\"networks\":[]}";
    if (!buf || !buf_len) return 0;
    size_t n = strlen(s);
    if (n >= buf_len) n = buf_len - 1;
    memcpy(buf, s, n); buf[n] = '\0';
    return n;
}
WEAK void web_hook_captive_connect(const char *ssid, const char *password) { (void)ssid; (void)password; }

WEAK bool web_hook_mqtt_connected(void) { return false; }
WEAK void web_hook_mqtt_publish_state(void) {}
WEAK void web_hook_mqtt_sources_changed(void) {}

WEAK bool web_hook_ha_rest_reapply_brightness(void) { return false; }

WEAK size_t web_hook_health_report_json(char *buf, size_t buf_len)
{
    static const char *fallback =
        "{\"status\":\"UNKNOWN\",\"message\":\"health subsystem not wired\"}";
    if (!buf || !buf_len) return 0;
    size_t n = strlen(fallback);
    if (n >= buf_len) n = buf_len - 1;
    memcpy(buf, fallback, n);
    buf[n] = '\0';
    return n;
}

WEAK bool  web_hook_system_healthy(void) { return true; }
WEAK float web_hook_system_temperature_c(void) { return 0.0f; }

WEAK size_t web_hook_bootlog_read(size_t offset, char *dst, size_t max_len)
{
    (void)offset;
    if (dst && max_len) dst[0] = '\0';
    return 0;
}
WEAK size_t web_hook_bootlog_size(void) { return 0; }
WEAK size_t web_hook_crash_dump(char *dst, size_t max_len) { if (dst && max_len) dst[0] = '\0'; return 0; }
WEAK size_t web_hook_logstream_read(char *dst, size_t max_len) { (void)dst; (void)max_len; return 0; }
WEAK void web_hook_crash_clear(void) {}
WEAK void web_hook_crash_save(void) {}
