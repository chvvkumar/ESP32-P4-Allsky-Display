/*
 * Periodic system monitor. See system_monitor.h.
 */

#include "system_monitor.h"
#include "system_watchdog.h"
#include "system_crash_log.h"
#include "app_config.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"

static const char *TAG = "sys_mon";

/* Cadences (system.md 6). */
#define MEM_CHECK_MS     30000
#define STACK_CHECK_MS   60000
#define SERIAL_FLUSH_MS   5000

/* Consecutive unhealthy memory checks that trigger a controlled reboot. */
#define UNHEALTHY_REBOOT_LIMIT  5

/* Named tasks whose stack high-water is logged. */
static const char *const MONITORED_TASKS[] = { "ImageDownloader", "HARestClient" };

static uint32_t s_heap_thresh;
static uint32_t s_psram_thresh;

static uint32_t s_min_free_heap;
static uint32_t s_min_free_psram;

static bool s_healthy = true;
static int  s_unhealthy_streak;

static int64_t s_last_mem_us;
static int64_t s_last_stack_us;
static int64_t s_last_flush_us;

static uint32_t free_heap(void)  { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
static uint32_t total_heap(void) { return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_INTERNAL); }
static uint32_t free_psram(void) { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }
static uint32_t total_psram(void){ return (uint32_t)heap_caps_get_total_size(MALLOC_CAP_SPIRAM); }

esp_err_t system_monitor_init(void)
{
    s_heap_thresh  = app_config_get_heap_thresh();
    s_psram_thresh = app_config_get_psram_thresh();

    s_min_free_heap  = free_heap();
    s_min_free_psram = free_psram();
    s_healthy = true;
    s_unhealthy_streak = 0;

    int64_t now = esp_timer_get_time();
    s_last_mem_us   = now;
    s_last_stack_us = now;
    s_last_flush_us = now;

    ESP_LOGI(TAG, "system monitor ready: heap>=%lu, psram>=%lu (free heap=%lu psram=%lu)",
             (unsigned long)s_heap_thresh, (unsigned long)s_psram_thresh,
             (unsigned long)s_min_free_heap, (unsigned long)s_min_free_psram);
    return ESP_OK;
}

static void memory_check(void)
{
    uint32_t fh = free_heap();
    uint32_t fp = free_psram();
    if (fh < s_min_free_heap)  s_min_free_heap = fh;
    if (fp < s_min_free_psram) s_min_free_psram = fp;

    bool low = (fh < s_heap_thresh) || (fp < s_psram_thresh);
    if (low) {
        s_healthy = false;
        s_unhealthy_streak++;
        ESP_LOGE(TAG, "[CRITICAL] Memory unhealthy (streak %d/%d): freeHeap=%lu freePsram=%lu "
                      "(thresholds heap %lu psram %lu)",
                 s_unhealthy_streak, UNHEALTHY_REBOOT_LIMIT,
                 (unsigned long)fh, (unsigned long)fp,
                 (unsigned long)s_heap_thresh, (unsigned long)s_psram_thresh);
        if (s_unhealthy_streak >= UNHEALTHY_REBOOT_LIMIT) {
            ESP_LOGE(TAG, "[CRITICAL] %d consecutive unhealthy checks; rebooting to recover",
                     s_unhealthy_streak);
            crash_log_mark_crash();
            crash_log_prepare_reboot();
            system_watchdog_delay_ms(100);
            esp_restart();
        }
    } else {
        if (!s_healthy) {
            ESP_LOGI(TAG, "Memory recovered: freeHeap=%lu freePsram=%lu",
                     (unsigned long)fh, (unsigned long)fp);
        }
        s_healthy = true;
        s_unhealthy_streak = 0;
    }
}

static void stack_check(void)
{
    UBaseType_t words = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGD(TAG, "stack high-water (main): %u words (%u bytes)",
             (unsigned)words, (unsigned)(words * sizeof(StackType_t)));
    if (words < 256) {
        ESP_LOGW(TAG, "main task stack high-water low: %u words", (unsigned)words);
    }
    for (size_t i = 0; i < sizeof(MONITORED_TASKS) / sizeof(MONITORED_TASKS[0]); i++) {
        TaskHandle_t h = xTaskGetHandle(MONITORED_TASKS[i]);
        if (!h) {
            continue;
        }
        UBaseType_t w = uxTaskGetStackHighWaterMark(h);
        ESP_LOGD(TAG, "stack high-water (%s): %u words (%u bytes)",
                 MONITORED_TASKS[i], (unsigned)w, (unsigned)(w * sizeof(StackType_t)));
        if (w < 256) {
            ESP_LOGW(TAG, "%s stack high-water low: %u words", MONITORED_TASKS[i], (unsigned)w);
        }
    }
}

void system_monitor_update(void)
{
    int64_t now = esp_timer_get_time();

    if (now - s_last_mem_us >= (int64_t)MEM_CHECK_MS * 1000) {
        s_last_mem_us = now;
        memory_check();
    }
    if (now - s_last_stack_us >= (int64_t)STACK_CHECK_MS * 1000) {
        s_last_stack_us = now;
        stack_check();
    }
    if (now - s_last_flush_us >= (int64_t)SERIAL_FLUSH_MS * 1000) {
        s_last_flush_us = now;
        fflush(stdout);
    }
}

bool system_monitor_is_healthy(void)      { return s_healthy; }
uint32_t system_monitor_free_heap(void)   { return free_heap(); }
uint32_t system_monitor_min_free_heap(void){ return s_min_free_heap; }
uint32_t system_monitor_total_heap(void)  { return total_heap(); }
uint32_t system_monitor_free_psram(void)  { return free_psram(); }
uint32_t system_monitor_min_free_psram(void){ return s_min_free_psram; }
uint32_t system_monitor_total_psram(void) { return total_psram(); }
