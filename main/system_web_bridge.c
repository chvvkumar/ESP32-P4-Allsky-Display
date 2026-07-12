/*
 * Strong implementations of the web-integration hooks that expose system-core
 * data (device health, memory-healthy flag, temperature, preserved crash/boot
 * log, live log stream, crash-log maintenance). These override the WEAK stubs in
 * web_integration_stubs.c so the corresponding web routes serve real data. The
 * web server owns the routes; this file only bridges them to the system modules.
 */

#include "web_integration.h"
#include "system_health.h"
#include "system_monitor.h"
#include "system_log_capture.h"
#include "system_crash_log.h"

#include <string.h>
#include <math.h>

#include "cJSON.h"

/* Link anchor: this object file provides only strong overrides of hooks that are
 * also weakly defined elsewhere, so nothing would otherwise reference a symbol
 * defined here and the linker would leave the strong definitions out of the image.
 * app_main() calls this once to force the object into the link. */
void system_web_bridge_link(void) { }

size_t web_hook_health_report_json(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) {
        return 0;
    }
    buf[0] = '\0';
    cJSON *root = system_health_build_json();
    if (!root) {
        return 0;
    }
    size_t n = 0;
    if (cJSON_PrintPreallocated(root, buf, (int)buf_len, 0)) {
        n = strlen(buf);
    }
    cJSON_Delete(root);
    return n;
}

bool web_hook_system_healthy(void)
{
    return system_monitor_is_healthy();
}

float web_hook_system_temperature_c(void)
{
    float t = system_health_temperature_c();
    return isnan(t) ? 0.0f : t;
}

size_t web_hook_bootlog_read(size_t offset, char *dst, size_t max_len)
{
    return log_capture_read(offset, dst, max_len);
}

size_t web_hook_bootlog_size(void)
{
    return log_capture_size();
}

/* Live console stream: keep a monotonic absolute cursor and return only bytes
 * appended since the previous call, clamping past overwritten data. */
size_t web_hook_logstream_read(char *dst, size_t max_len)
{
    static uint64_t s_cursor;
    if (!dst || max_len == 0) {
        return 0;
    }
    uint64_t total = log_capture_total();
    size_t   size  = log_capture_size();
    uint64_t oldest = total - size;

    if (s_cursor < oldest) {
        s_cursor = oldest;   /* bytes were overwritten before we read them */
    }
    if (s_cursor >= total) {
        return 0;            /* nothing new */
    }
    size_t rel = (size_t)(s_cursor - oldest);
    size_t n = log_capture_read(rel, dst, max_len);
    s_cursor += n;
    return n;
}

void web_hook_crash_clear(void)
{
    crash_log_clear_all();
}

void web_hook_crash_save(void)
{
    crash_log_prepare_reboot();
}
