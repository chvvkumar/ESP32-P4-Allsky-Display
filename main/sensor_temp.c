/*
 * Single owner of the ESP32-P4 internal temperature sensor. See sensor_temp.h.
 */

#include "sensor_temp.h"

#include <math.h>

#include "freertos/FreeRTOS.h"
#include "driver/temperature_sensor.h"

/* ---- Shared sensor (lazy) ---------------------------------------------- */
static temperature_sensor_handle_t s_tsens;
static portMUX_TYPE s_install_lock = portMUX_INITIALIZER_UNLOCKED;

float sensor_temp_read_celsius(void)
{
    if (!s_tsens) {
        /* Serialize the one-time install so concurrent first callers do not
         * both invoke temperature_sensor_install (only one instance allowed). */
        portENTER_CRITICAL(&s_install_lock);
        bool need_install = (s_tsens == NULL);
        portEXIT_CRITICAL(&s_install_lock);

        if (need_install) {
            temperature_sensor_handle_t h = NULL;
            /* Range must map to a single predefined hardware range; a span like
             * (-10,120) fits none and fails with "Out of testing range".
             * (-10,80) matches a valid P4 range and covers normal SoC temps. */
            temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
            if (temperature_sensor_install(&cfg, &h) != ESP_OK) {
                return NAN;
            }
            if (temperature_sensor_enable(h) != ESP_OK) {
                temperature_sensor_uninstall(h);
                return NAN;
            }

            portENTER_CRITICAL(&s_install_lock);
            if (s_tsens == NULL) {
                s_tsens = h;
                h = NULL;
            }
            portEXIT_CRITICAL(&s_install_lock);

            /* Lost the race: another caller installed first. Drop our copy. */
            if (h) {
                temperature_sensor_disable(h);
                temperature_sensor_uninstall(h);
            }
        }
    }

    float t = NAN;
    if (temperature_sensor_get_celsius(s_tsens, &t) != ESP_OK) {
        return NAN;
    }
    return t;
}
