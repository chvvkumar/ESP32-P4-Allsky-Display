#pragma once

/*
 * Interactive single-key serial command interpreter.
 *
 * Each command is one ASCII character; letter commands are case-insensitive and
 * take no arguments. Commands cover live image-transform nudging (scale/pan/
 * rotate, auto-persisted with debounced flash writes), image navigation,
 * brightness, diagnostic reports, reboot, and additions for firmware version,
 * factory reset, and clearing crash logs.
 *
 * A dedicated console task reads the primary USB serial console and dispatches
 * one command per received byte. system_serial_dispatch() is also exported so the
 * main loop can drive dispatch directly if preferred.
 *
 * Live values and side effects this module cannot perform itself (backlight,
 * image scheduling, re-render, web-server restart, and the network/PPA report
 * text) are supplied through the hooks struct registered at init.
 */

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Apply backlight brightness live, 0..100 (not persisted). NULL: no-op. */
    void (*apply_brightness)(int pct);
    /* Current backlight brightness 0..100. NULL: falls back to configured default. */
    int  (*get_brightness)(void);
    /* Advance to the next image (same semantics as automatic cycling: reset the
     * cycle timer and force an immediate download). NULL: no-op. */
    void (*next_image)(void);
    /* Force an immediate re-download/redisplay of the current image. NULL: no-op. */
    void (*force_refresh)(void);
    /* Re-render the current image from its cached source buffer (no download),
     * after a transform change. NULL: no-op. */
    void (*rerender_current)(void);
    /* Stop and restart the web server on port 8080 (serial 'X'). NULL: no-op. */
    void (*restart_web_server)(void);
    /* Write a web-server/WiFi status line for 'X'. NULL: a minimal line is used. */
    void (*web_status)(char *out, size_t len);
    /* Write the network diagnostic block for 'I'. NULL: a minimal line is used. */
    void (*network_info)(char *out, size_t len);
    /* Write the PPA accelerator diagnostic block for 'P'. NULL: a minimal line. */
    void (*ppa_info)(char *out, size_t len);
} system_serial_hooks_t;

/* Register hooks and initialize the debounced-save helper. `hooks` may be NULL.
 * Call once at boot after app_config_init(). */
esp_err_t system_serial_init(const system_serial_hooks_t *hooks);

/* Create the console reader task (core 1, low priority). Idempotent. */
esp_err_t system_serial_start(void);

/* Dispatch a single command byte. Safe to call from the main loop or the console
 * task. Unknown characters are ignored silently. */
void system_serial_dispatch(char c);

#ifdef __cplusplus
}
#endif
