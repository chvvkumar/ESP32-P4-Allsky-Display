idf_component_register(
    SRCS
        "main.c"
        "integration_hooks.c"
        "display_defs.c"
        "app_config.c"
        "config_backup.c"
        "config_debounce.c"
        # System core
        "system_crash_log.c"
        "system_log_capture.c"
        "system_watchdog.c"
        "system_monitor.c"
        "system_health.c"
        "system_serial.c"
        "system_tasks.c"
        "system_web_bridge.c"
        # Network
        "network_wifi.c"
        "network_time.c"
        "network_portal.c"
        "network_poll.c"
        "network_http.c"
        "network_timeparse.c"
        # Image pipeline
        "image_pipeline.c"
        "image_download.c"
        "image_decode.c"
        "image_transform.c"
        "image_ppa.c"
        "image_sink.c"
        # Moon / overlays
        "moon_ephemeris.c"
        "moon_sphere.cpp"
        "moon_interaction.c"
        "moon_source.c"
        # Web server
        "web_server.c"
        "web_pages.c"
        "web_api_read.c"
        "web_api_config.c"
        "web_api_images.c"
        "web_ws.c"
        "web_screenshot.c"
        "web_ota.c"
        "web_captive.c"
        "web_route_auth.c"
        "web_util.c"
        "web_integration_stubs.c"
        # MQTT / input / brightness
        "mqtt_ha.c"
        "input_touch.c"
        "brightness_ctrl.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        json
        esp_timer
        esp_driver_tsens
        esp_driver_uart
        esp_app_format
        app_update
        esp_hw_support
        esp_wifi
        esp_wifi_remote
        esp_netif
        esp_event
        esp_http_server
        esp_http_client
        esp-tls
        lwip
        mqtt
        esp_driver_jpeg
        esp_driver_ppa
        esp_lvgl_port
        esp_mm
        driver
        mbedtls
        esp_partition
        spi_flash
        lvgl
        tgx
        esp32_p4_wifi6_touch_lcd_xc
    EMBED_FILES
        "moon_equirect.rgb565"
        "web_assets/app.html"
        "web_assets/login.html"
    EMBED_TXTFILES
        "network_setup_ui.html"
)

# On-device panic banner + backtrace capture: --wrap intercepts the panic
# character sink so __wrap_panic_print_char() mirrors it into an RTC_NOINIT ring.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=panic_print_char")

# HA entity labels/topics are safely clamped by snprintf; the theoretical
# truncation on large image indices is benign, and %d/%ld are identical on
# rv32 (int and int32_t are both 32-bit), so these must not fail the build.
set_source_files_properties(mqtt_ha.c PROPERTIES
    COMPILE_OPTIONS "-Wno-error=format-truncation;-Wno-error=format")

# LVGL upstream quirks also worked around in the NINA-Display donor.
idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB}
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
        -Wno-attributes       # conflicting section attrs in lv_color/lv_math (upstream LVGL)
        -Wno-unused-function  # unused atomic.h inlines in lv_freertos.c (upstream ESP-IDF)
)
