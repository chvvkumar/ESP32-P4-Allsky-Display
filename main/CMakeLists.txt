idf_component_register(
    SRCS
        "main.c"
        "app_config.c"
        "config_backup.c"
        "config_debounce.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        json
        esp_timer
)

# LVGL upstream quirks also worked around in the NINA-Display donor.
idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -Wno-attributes       # conflicting section attrs in lv_color/lv_math (upstream LVGL)
        -Wno-unused-function  # unused atomic.h inlines in lv_freertos.c (upstream ESP-IDF)
)
