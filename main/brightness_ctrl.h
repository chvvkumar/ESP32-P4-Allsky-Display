#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Backlight brightness policy.
 *
 * A single clamp-and-apply funnel drives the LCD backlight (0-100%). The physical
 * PWM (GPIO26, 5 kHz, 10-bit, hardware-inverted) is owned by the BSP; this module
 * clamps, applies through the BSP, and caches the value. Every brightness source
 * routes through brightness_apply():
 *   - boot default   : brightness_init() / brightness_apply_config_default()
 *   - web UI save     : caller sets defaultBrightness then brightness_apply(pct)
 *   - serial +/-      : brightness_step()
 *   - MQTT command    : mqtt_ha apply_brightness hook -> brightness_apply()
 *   - HA REST sensor  : mqtt_ha REST loop -> brightness_apply() (not persisted)
 *
 * brightness_apply() and brightness_get() are the exact hooks mqtt_ha_init()
 * expects (apply_brightness / get_brightness).
 */

/* Apply the persisted defaultBrightness to the backlight and cache it. Call once
 * after app_config_init() and after bsp_display_start() (the BSP has configured
 * the LEDC channel by then). */
esp_err_t brightness_init(void);

/* The funnel: clamp to [0,100], drive the backlight, cache. Does not persist. */
void brightness_apply(int pct);

/* Current cached brightness percentage. */
int brightness_get(void);

/* Re-read defaultBrightness from config and apply it. */
void brightness_apply_config_default(void);

/* Serial console step: adjust by delta (typically +/-10), clamp to [0,100], apply
 * immediately. NOT persisted (volatile until reboot). */
void brightness_step(int delta);

/* Force-update endpoint (input-display 4.4): re-apply the mode-appropriate value.
 * HA REST mode re-applies the last sensor-derived brightness; MQTT auto mode
 * republishes current state to Home Assistant; manual mode applies the persisted
 * default. */
void brightness_reapply_active_mode(void);

/* Brightness control mode selection with mutual exclusion (input-display 4.5).
 * Enabling either automatic mode disables the other; both flags are persisted.
 * Used by the web UI and serial console (the MQTT auto_brightness switch handles
 * its own mutual exclusion inside mqtt_ha). */
void brightness_set_auto_mode(bool on);   /* MQTT brightness control */
void brightness_set_ha_rest(bool on);     /* HA REST light-sensor control */

#ifdef __cplusplus
}
#endif
