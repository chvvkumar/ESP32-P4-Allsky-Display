#pragma once

/*
 * Single owner of the ESP32-P4 internal temperature sensor.
 *
 * ESP-IDF permits only one temperature_sensor instance; installing a second
 * returns ESP_ERR_INVALID_STATE. This module lazy-installs one shared sensor on
 * first read and hands the value to every caller (system_health, mqtt_ha, ...).
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads the SoC die temperature in Celsius. Lazy-installs and enables the
 * single hardware sensor on first call. Returns NAN on install or read
 * failure. */
float sensor_temp_read_celsius(void);

#ifdef __cplusplus
}
#endif
