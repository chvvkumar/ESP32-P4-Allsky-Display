/*
 * Single-key serial command interpreter + console task. See system_serial.h.
 */

#include "system_serial.h"
#include "system_health.h"
#include "system_monitor.h"
#include "system_crash_log.h"
#include "config_debounce.h"
#include "app_config.h"
#include "mqtt_ha.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#include "sdkconfig.h"

static const char *TAG = "serial_cmd";

/* Transform constants (serial-commands.md 2). */
#define SCALE_STEP    0.1f
#define SCALE_MIN     0.1f
#define SCALE_MAX     2.0f
#define MOVE_STEP     10
#define ROT_STEP      90.0f
#define DEF_SCALE     1.2f

/* Debounced flash write coalescing window for rapid transform nudges. */
#define SAVE_DEBOUNCE_MS  1500

#define CONSOLE_TASK_STACK  4096
#define CONSOLE_TASK_PRIO   2

static system_serial_hooks_t s_hooks;
static config_debounce_t     s_save_debounce;
static int  s_brightness = -1;   /* lazily seeded */
static bool s_task_started;

/* ---- helpers ----------------------------------------------------------- */
static void save_flush(void *arg)
{
    (void)arg;
    app_config_save();
}

static int current_index(void)
{
    int idx = (int)app_config_get_curr_img_idx();
    int cnt = app_config_source_count();
    if (idx < 0 || idx >= cnt) {
        idx = 0;
    }
    return idx;
}

static bool load_transform(int idx, image_transform_t *tf)
{
    image_source_t src;
    if (!app_config_get_source(idx, &src)) {
        return false;
    }
    *tf = src.transform;
    return true;
}

/* Persist a mutated transform (debounced) and re-render. */
static void commit_transform(int idx, const image_transform_t *tf)
{
    app_config_set_source_transform(idx, tf);
    config_debounce_touch(&s_save_debounce);
    if (s_hooks.rerender_current) {
        s_hooks.rerender_current();
    }
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int seed_brightness(void)
{
    if (s_brightness < 0) {
        if (s_hooks.get_brightness) {
            s_brightness = s_hooks.get_brightness();
        } else {
            s_brightness = (int)app_config_get_def_bright();
        }
        if (s_brightness < 0) s_brightness = 0;
        if (s_brightness > 100) s_brightness = 100;
    }
    return s_brightness;
}

/* ---- command handlers -------------------------------------------------- */
static void cmd_scale(float delta)
{
    int idx = current_index();
    image_transform_t tf;
    if (!load_transform(idx, &tf)) return;
    tf.scale_x = clampf(tf.scale_x + delta, SCALE_MIN, SCALE_MAX);
    tf.scale_y = clampf(tf.scale_y + delta, SCALE_MIN, SCALE_MAX);
    commit_transform(idx, &tf);
    ESP_LOGI(TAG, "Scale %s: %.1fx%.1f (saved for image %d)",
             delta > 0 ? "increased" : "decreased", tf.scale_x, tf.scale_y, idx + 1);
}

static void cmd_move(int dx, int dy)
{
    int idx = current_index();
    image_transform_t tf;
    if (!load_transform(idx, &tf)) return;
    tf.offset_x += dx;
    tf.offset_y += dy;
    commit_transform(idx, &tf);
    const char *dir = dy < 0 ? "up" : dy > 0 ? "down" : dx < 0 ? "left" : "right";
    ESP_LOGI(TAG, "Move %s: offset=%ld,%ld (saved for image %d)",
             dir, (long)tf.offset_x, (long)tf.offset_y, idx + 1);
}

static void cmd_rotate(float delta)
{
    int idx = current_index();
    image_transform_t tf;
    if (!load_transform(idx, &tf)) return;
    tf.rotation += delta;
    if (tf.rotation < 0.0f)     tf.rotation += 360.0f;
    if (tf.rotation >= 360.0f)  tf.rotation -= 360.0f;
    commit_transform(idx, &tf);
    ESP_LOGI(TAG, "Rotate %s: %.0f\xC2\xB0 (saved for image %d)",
             delta < 0 ? "CCW" : "CW", tf.rotation, idx + 1);
}

static void cmd_reset_transforms(void)
{
    int idx = current_index();
    ESP_LOGI(TAG, "Resetting transforms for image %d", idx + 1);
    image_transform_t tf = {
        .scale_x = DEF_SCALE, .scale_y = DEF_SCALE,
        .offset_x = 0, .offset_y = 0, .rotation = 0.0f,
    };
    commit_transform(idx, &tf);
    ESP_LOGI(TAG, "All transformations reset to defaults");
}

static void cmd_save_transforms(void)
{
    int idx = current_index();
    image_transform_t tf;
    if (!load_transform(idx, &tf)) return;
    /* Re-apply (idempotent) and flush immediately, no re-render. */
    app_config_set_source_transform(idx, &tf);
    config_debounce_flush_now(&s_save_debounce);
    printf("Saved transform settings for image %d: scale=%.1fx%.1f, offset=%ld,%ld, rotation=%.0f\xC2\xB0\n",
           idx + 1, tf.scale_x, tf.scale_y,
           (long)tf.offset_x, (long)tf.offset_y, tf.rotation);
}

static void cmd_next_image(void)
{
    if (!app_config_get_cycling_en() || app_config_source_count() <= 1) {
        ESP_LOGW(TAG, "Next image command ignored - cycling not enabled or only one source configured");
        return;
    }
    if (s_hooks.next_image) {
        s_hooks.next_image();
    }
}

static void cmd_force_refresh(void)
{
    ESP_LOGI(TAG, "Force image refresh requested");
    if (s_hooks.force_refresh) {
        s_hooks.force_refresh();
    }
}

static void cmd_brightness(int delta)
{
    int b = seed_brightness() + delta;
    if (b < 0) b = 0;
    if (b > 100) b = 100;
    s_brightness = b;
    if (s_hooks.apply_brightness) {
        s_hooks.apply_brightness(b);
    }
    ESP_LOGI(TAG, "Brightness set to %d%% (not saved)", b);
}

static void cmd_reboot(void)
{
    ESP_LOGW(TAG, "Device reboot requested via serial command");
    vTaskDelay(pdMS_TO_TICKS(1000));
    crash_log_prepare_reboot();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void cmd_memory_info(void)
{
    ESP_LOGI(TAG, "===== MEMORY =====");
    ESP_LOGI(TAG, "free heap:  %lu B (min %lu)",
             (unsigned long)system_monitor_free_heap(),
             (unsigned long)system_monitor_min_free_heap());
    ESP_LOGI(TAG, "free psram: %lu B (min %lu)",
             (unsigned long)system_monitor_free_psram(),
             (unsigned long)system_monitor_min_free_psram());
    ESP_LOGI(TAG, "health: %s", system_monitor_is_healthy() ? "HEALTHY" : "CRITICAL");
    ESP_LOGI(TAG, "==================");
}

static void cmd_network_info(void)
{
    char buf[512];
    if (s_hooks.network_info) {
        s_hooks.network_info(buf, sizeof(buf));
        ESP_LOGI(TAG, "===== NETWORK =====\n%s", buf);
    } else {
        ESP_LOGI(TAG, "Network info unavailable");
    }
}

static void cmd_ppa_info(void)
{
    char buf[256];
    if (s_hooks.ppa_info) {
        s_hooks.ppa_info(buf, sizeof(buf));
        ESP_LOGI(TAG, "===== PPA =====\n%s", buf);
    } else {
        ESP_LOGI(TAG, "PPA info unavailable");
    }
}

static void cmd_mqtt_info(void)
{
    char buf[512];
    mqtt_ha_connection_info(buf, sizeof(buf));
    ESP_LOGI(TAG, "===== MQTT =====\n%s", buf);
}

static void cmd_web_status(void)
{
    char buf[256];
    if (s_hooks.web_status) {
        s_hooks.web_status(buf, sizeof(buf));
        printf("%s\n", buf);
    }
    if (s_hooks.restart_web_server) {
        s_hooks.restart_web_server();
    }
}

static void cmd_health(void)
{
    char *buf = malloc(2048);
    if (!buf) {
        return;
    }
    system_health_text_report(buf, 2048);
    ESP_LOGI(TAG, "\n%s", buf);
    free(buf);
}

static void cmd_version(void)
{
    const esp_app_desc_t *d = esp_app_get_description();
    printf("Firmware: %s\nVersion:  %s\nBuilt:    %s %s\nIDF:      %s\nBoot #%lu\n",
           d ? d->project_name : "?", d ? d->version : "?",
           d ? d->date : "?", d ? d->time : "?", d ? d->idf_ver : "?",
           (unsigned long)crash_log_boot_count());
}

static void cmd_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset requested via serial command");
    vTaskDelay(pdMS_TO_TICKS(1000));
    app_config_factory_reset();
    crash_log_prepare_reboot();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

static void cmd_clear_crash_logs(void)
{
    crash_log_clear_all();
    ESP_LOGI(TAG, "Crash logs cleared via serial command");
}

static void cmd_help(void)
{
    static const char *help =
        "\n===== AllSky Display serial commands (115200 baud, case-insensitive) =====\n"
        "Navigation : N next image   F force refresh\n"
        "Scaling    : + scale up      - scale down\n"
        "Movement   : W up  S down    A left  D right\n"
        "Rotation   : Q rotate 90 CCW E rotate 90 CW\n"
        "Reset      : R reset transforms  V save transforms\n"
        "Brightness : L up  K down    (not persisted)\n"
        "System     : B reboot  M memory  I network  P ppa  T mqtt  X web status/restart  G health\n"
        "             O firmware version  Z factory reset  C clear crash logs\n"
        "Touch      : single tap = next image, double tap = toggle single-image refresh\n"
        "Help       : H or ?\n"
        "=========================================================================\n";
    printf("%s", help);
}

void system_serial_dispatch(char c)
{
    switch (c) {
        case '+':            cmd_scale(+SCALE_STEP); break;
        case '-':            cmd_scale(-SCALE_STEP); break;
        case 'w': case 'W':  cmd_move(0, -MOVE_STEP); break;
        case 's': case 'S':  cmd_move(0, +MOVE_STEP); break;
        case 'a': case 'A':  cmd_move(-MOVE_STEP, 0); break;
        case 'd': case 'D':  cmd_move(+MOVE_STEP, 0); break;
        case 'q': case 'Q':  cmd_rotate(-ROT_STEP); break;
        case 'e': case 'E':  cmd_rotate(+ROT_STEP); break;
        case 'r': case 'R':  cmd_reset_transforms(); break;
        case 'v': case 'V':  cmd_save_transforms(); break;
        case 'n': case 'N':  cmd_next_image(); break;
        case 'f': case 'F':  cmd_force_refresh(); break;
        case 'l': case 'L':  cmd_brightness(+10); break;
        case 'k': case 'K':  cmd_brightness(-10); break;
        case 'b': case 'B':  cmd_reboot(); break;
        case 'm': case 'M':  cmd_memory_info(); break;
        case 'i': case 'I':  cmd_network_info(); break;
        case 'p': case 'P':  cmd_ppa_info(); break;
        case 't': case 'T':  cmd_mqtt_info(); break;
        case 'x': case 'X':  cmd_web_status(); break;
        case 'g': case 'G':  cmd_health(); break;
        case 'o': case 'O':  cmd_version(); break;
        case 'z': case 'Z':  cmd_factory_reset(); break;
        case 'c': case 'C':  cmd_clear_crash_logs(); break;
        case 'h': case 'H': case '?': cmd_help(); break;
        default: break;   /* unknown: ignored silently */
    }
}

/* ---- console task ------------------------------------------------------ */
#define CONSOLE_POLL_MS  100

static void console_task(void *arg)
{
    (void)arg;
    for (;;) {
        char c;
        int n = read(STDIN_FILENO, &c, 1);
        if (n == 1) {
            system_serial_dispatch(c);
            /* Drain any remaining buffered bytes (one command per read). */
            char drain[32];
            while (read(STDIN_FILENO, drain, sizeof(drain)) > 0) {
                /* discard */
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(CONSOLE_POLL_MS));
        }
    }
}

esp_err_t system_serial_init(const system_serial_hooks_t *hooks)
{
    if (hooks) {
        s_hooks = *hooks;
    } else {
        memset(&s_hooks, 0, sizeof(s_hooks));
    }
    esp_err_t err = config_debounce_init(&s_save_debounce, SAVE_DEBOUNCE_MS, save_flush, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "debounce init failed: %s (transforms will save immediately)",
                 esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "serial command interpreter ready");
    return ESP_OK;
}

esp_err_t system_serial_start(void)
{
    if (s_task_started) {
        return ESP_OK;
    }
    /* Drive stdin from the console UART so single-key commands arrive on the
     * primary UART (COM8). The bootloader/app logs already use this UART. */
    esp_err_t err = uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "console uart driver install failed: %s", esp_err_to_name(err));
        return err;
    }
    uart_vfs_dev_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    /* Non-blocking reads so the poll loop yields instead of busy-spinning. */
    int fd = fileno(stdin);
    if (fd >= 0) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
    }
    if (xTaskCreate(console_task, "SerialConsole", CONSOLE_TASK_STACK, NULL,
                    CONSOLE_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "console task create failed");
        return ESP_ERR_NO_MEM;
    }
    s_task_started = true;
    ESP_LOGI(TAG, "serial console task started");
    return ESP_OK;
}
