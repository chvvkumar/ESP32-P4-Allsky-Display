/*
 * Three-tier crash logging + panic-text capture. See system_crash_log.h.
 */

#include "system_crash_log.h"
#include "system_log_capture.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_mac.h"
#include "esp_chip_info.h"
#include "esp_clk_tree.h"
#include "nvs.h"

static const char *TAG = "crash_log";

/* ---- Tier sizes (system.md 8.1) ---------------------------------------- */
#define RAM_RING_SIZE   8192
#define RTC_RING_SIZE   4096
#define NVS_BLOB_MAX    4000
#define CRASH_NS        "crash_log"

/* ---- Panic-text capture (linker --wrap) -------------------------------- */
#define PANIC_BUF_SIZE  3072
#define PANIC_MAGIC     0xC7A54106u

typedef struct {
    uint32_t magic;
    uint32_t len;
    uint32_t head;
    char     buf[PANIC_BUF_SIZE];
} panic_rtc_t;

static RTC_NOINIT_ATTR panic_rtc_t s_panic_rtc;

/* Provided by the linker (-Wl,--wrap=panic_print_char). */
extern void __real_panic_print_char(char c);

void __wrap_panic_print_char(char c)
{
    if (s_panic_rtc.magic != PANIC_MAGIC) {
        s_panic_rtc.magic = PANIC_MAGIC;
        s_panic_rtc.len   = 0;
        s_panic_rtc.head  = 0;
    }
    s_panic_rtc.buf[s_panic_rtc.head] = c;
    s_panic_rtc.head = (s_panic_rtc.head + 1u) % PANIC_BUF_SIZE;
    s_panic_rtc.len++;
    __real_panic_print_char(c);
}

static size_t panic_text_extract(char *out, size_t out_size)
{
    if (out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (s_panic_rtc.magic != PANIC_MAGIC || s_panic_rtc.len == 0) {
        return 0;
    }
    size_t total = s_panic_rtc.len;
    size_t start;
    if (total <= PANIC_BUF_SIZE) {
        start = 0;
    } else {
        total = PANIC_BUF_SIZE;
        start = s_panic_rtc.head;
    }
    size_t copy = total < (out_size - 1) ? total : (out_size - 1);
    for (size_t i = 0; i < copy; i++) {
        out[i] = s_panic_rtc.buf[(start + i) % PANIC_BUF_SIZE];
    }
    out[copy] = '\0';
    return copy;
}

static void panic_text_clear(void)
{
    s_panic_rtc.magic = 0;
    s_panic_rtc.len   = 0;
    s_panic_rtc.head  = 0;
}

/* ---- RTC ring (survives soft reset / crash) ---------------------------- */
#define RTC_MAGIC 0xA115C0DEu

typedef struct {
    uint32_t magic;
    uint32_t boot_count;   /* mirror of NVS boot count */
    uint32_t crash_marker; /* nonzero forces crash classification next boot */
    uint32_t head;
    uint32_t count;
    char     buf[RTC_RING_SIZE];
} rtc_log_t;

static RTC_NOINIT_ATTR rtc_log_t s_rtc;

/* ---- RAM ring (current session) ---------------------------------------- */
static char  *s_ram;
static size_t s_ram_head;
static size_t s_ram_count;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* ---- Boot classification state ----------------------------------------- */
static bool        s_last_crash;
static bool        s_reset_wdt;
static esp_reset_reason_t s_reason;
static vprintf_like_t s_orig_vprintf;

static const char *reason_name(esp_reset_reason_t r)
{
    switch (r) {
        case ESP_RST_POWERON:   return "Power-on";
        case ESP_RST_EXT:       return "External reset";
        case ESP_RST_SW:        return "Software reset";
        case ESP_RST_PANIC:     return "Panic / exception";
        case ESP_RST_INT_WDT:   return "Interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "Task watchdog";
        case ESP_RST_WDT:       return "Other watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep-sleep wake";
        case ESP_RST_BROWNOUT:  return "Brownout";
        case ESP_RST_SDIO:      return "SDIO reset";
        default:                return "Unknown";
    }
}

/* ---- Generic ring append (buf/head/count in physical bytes) ------------ */
static void ring_append(char *buf, size_t cap, size_t *head, size_t *count,
                        const char *src, size_t len)
{
    if (len == 0 || !buf) {
        return;
    }
    if (len > cap) {          /* keep the tail */
        src += (len - cap);
        len = cap;
    }
    size_t first = cap - *head;
    if (first > len) {
        first = len;
    }
    memcpy(buf + *head, src, first);
    size_t second = len - first;
    if (second) {
        memcpy(buf, src + first, second);
    }
    *head = (*head + len) % cap;
    *count += len;
    if (*count > cap) {
        *count = cap;
    }
}

static size_t ring_read(const char *buf, size_t cap, size_t head, size_t count,
                        char *dst, size_t max_len)
{
    if (!buf || !dst || max_len == 0 || count == 0) {
        if (dst && max_len) {
            dst[0] = '\0';
        }
        return 0;
    }
    size_t want = (count < max_len - 1) ? count : max_len - 1;
    size_t start = (head + cap - count) % cap;
    size_t first = cap - start;
    if (first > want) {
        first = want;
    }
    memcpy(dst, buf + start, first);
    size_t second = want - first;
    if (second) {
        memcpy(dst + first, buf, second);
    }
    dst[want] = '\0';
    return want;
}

/* Fan a line into both RAM and RTC rings under the spinlock. */
static void tiers_append(const char *src, size_t len)
{
    portENTER_CRITICAL(&s_mux);
    if (s_ram) {
        ring_append(s_ram, RAM_RING_SIZE, &s_ram_head, &s_ram_count, src, len);
    }
    if (s_rtc.magic == RTC_MAGIC) {
        size_t h = s_rtc.head, c = s_rtc.count;
        ring_append(s_rtc.buf, RTC_RING_SIZE, &h, &c, src, len);
        s_rtc.head = (uint32_t)h;
        s_rtc.count = (uint32_t)c;
    }
    portEXIT_CRITICAL(&s_mux);
}

/* ---- vprintf hook: every log line feeds the tiers ---------------------- */
static int crash_vprintf(const char *fmt, va_list args)
{
    char line[256];
    va_list cp;
    va_copy(cp, args);
    int n = vsnprintf(line, sizeof(line), fmt, cp);
    va_end(cp);
    if (n > 0) {
        size_t to_copy = (size_t)n;
        if (to_copy >= sizeof(line)) {
            to_copy = sizeof(line) - 1;
        }
        tiers_append(line, to_copy);
    }
    if (s_orig_vprintf) {
        return s_orig_vprintf(fmt, args);
    }
    return vprintf(fmt, args);
}

/* ---- NVS persistence --------------------------------------------------- */
static void nvs_save_rtc(void)
{
    nvs_handle_t h;
    if (nvs_open(CRASH_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    /* Only the newest NVS_BLOB_MAX bytes of the RTC ring are saved. */
    char *tmp = malloc(NVS_BLOB_MAX + 1);
    if (tmp) {
        size_t n;
        portENTER_CRITICAL(&s_mux);
        n = ring_read(s_rtc.buf, RTC_RING_SIZE, s_rtc.head, s_rtc.count,
                      tmp, NVS_BLOB_MAX + 1);
        portEXIT_CRITICAL(&s_mux);
        nvs_set_blob(h, "log_data", tmp, n);
        free(tmp);
    }
    nvs_set_u32(h, "log_boot", s_rtc.boot_count);
    nvs_set_u32(h, "log_time", (uint32_t)(esp_timer_get_time() / 1000));
    time_t now = time(NULL);
    if (now >= 1577836800) {  /* wall time valid */
        nvs_set_u32(h, "log_timestamp", (uint32_t)now);
    }
    nvs_commit(h);
    nvs_close(h);
}

/* ---- Boot counter (NVS master, RTC mirror) ----------------------------- */
static uint32_t boot_counter_increment(void)
{
    nvs_handle_t h;
    uint32_t bc = 0;
    if (nvs_open(CRASH_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_get_u32(h, "boot_count", &bc);
        bc++;
        nvs_set_u32(h, "boot_count", bc);
        nvs_commit(h);
        nvs_close(h);
    }
    return bc;
}

/* ---- Public init ------------------------------------------------------- */
void crash_log_init(void)
{
    s_ram = heap_caps_malloc(RAM_RING_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_ram_head = 0;
    s_ram_count = 0;

    /* Validate / initialise the RTC ring (invalid after power loss). */
    bool rtc_fresh = (s_rtc.magic != RTC_MAGIC);
    if (rtc_fresh) {
        s_rtc.magic = RTC_MAGIC;
        s_rtc.boot_count = 0;
        s_rtc.crash_marker = 0;
        s_rtc.head = 0;
        s_rtc.count = 0;
    }

    s_reason = esp_reset_reason();
    s_reset_wdt = (s_reason == ESP_RST_TASK_WDT ||
                   s_reason == ESP_RST_INT_WDT ||
                   s_reason == ESP_RST_WDT);
    bool reason_crash = (s_reason == ESP_RST_PANIC) || s_reset_wdt;
    s_last_crash = reason_crash || (s_rtc.crash_marker != 0);

    uint32_t bc = boot_counter_increment();
    s_rtc.boot_count = bc;

    /* Install the capture hook AFTER any earlier hook (log_capture) so we chain. */
    s_orig_vprintf = esp_log_set_vprintf(crash_vprintf);

    /* Boot banner into the tiers (8.5). */
    {
        char banner[96];
        time_t now = time(NULL);
        if (now >= 1577836800) {
            struct tm tmv;
            gmtime_r(&now, &tmv);
            char ts[24];
            strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
            snprintf(banner, sizeof(banner), "===== BOOT #%lu at %lld ms [%s] =====\n",
                     (unsigned long)bc, (long long)(esp_timer_get_time() / 1000), ts);
        } else {
            snprintf(banner, sizeof(banner), "===== BOOT #%lu at %lld ms =====\n",
                     (unsigned long)bc, (long long)(esp_timer_get_time() / 1000));
        }
        tiers_append(banner, strlen(banner));
    }

    if (s_last_crash) {
        char *panic = malloc(PANIC_BUF_SIZE + 1);
        if (panic) {
            panic_text_extract(panic, PANIC_BUF_SIZE + 1);
        }

        esp_chip_info_t chip;
        esp_chip_info(&chip);
        uint32_t cpu_hz = 0, xtal_hz = 0;
        esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU,
                ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &cpu_hz);
        esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_XTAL,
                ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &xtal_hz);

        char rec[512];
        int len = snprintf(rec, sizeof(rec),
            "[CRASH] Previous boot ended abnormally: %s\n"
            "  cpu=%luMHz xtal=%luMHz freeHeap=%lu largestBlock=%lu\n"
            "  freePsram=%lu totalPsram=%lu\n"
            "  (backtrace captured below; also available from live serial)\n",
            reason_name(s_reason),
            (unsigned long)(cpu_hz / 1000000), (unsigned long)(xtal_hz / 1000000),
            (unsigned long)esp_get_free_heap_size(),
            (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
            (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            (unsigned long)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        if (len > 0) {
            tiers_append(rec, (size_t)len);
        }
        if (panic && panic[0]) {
            tiers_append("[PANIC] ", 8);
            tiers_append(panic, strlen(panic));
            tiers_append("\n", 1);
        }
        free(panic);

        /* Persist the RTC ring (with the crash record) to NVS, then clear markers. */
        nvs_save_rtc();
        s_rtc.crash_marker = 0;
        panic_text_clear();

        ESP_LOGE(TAG, "**********************************************************");
        ESP_LOGE(TAG, "* PREVIOUS BOOT ENDED IN A CRASH (%s)", reason_name(s_reason));
        ESP_LOGE(TAG, "* Crash logs preserved. See the web console / logs.");
        ESP_LOGE(TAG, "**********************************************************");
    } else {
        const esp_app_desc_t *desc = esp_app_get_description();
        char rec[192];
        int len = snprintf(rec, sizeof(rec),
            "[BOOT] Normal boot (%s). fw=%s built %s %s\n",
            reason_name(s_reason), desc ? desc->version : "?",
            desc ? desc->date : "?", desc ? desc->time : "?");
        if (len > 0) {
            tiers_append(rec, (size_t)len);
        }
        panic_text_clear();
    }

    ESP_LOGI(TAG, "Crash logger ready: boot #%lu, reason=%s, lastCrash=%s",
             (unsigned long)bc, reason_name(s_reason), s_last_crash ? "yes" : "no");
}

/* ---- Public accessors -------------------------------------------------- */
bool crash_log_last_boot_was_crash(void) { return s_last_crash; }
bool crash_log_reset_was_watchdog(void)  { return s_reset_wdt; }
uint32_t crash_log_boot_count(void)      { return s_rtc.boot_count; }
const char *crash_log_reset_reason_name(void) { return reason_name(s_reason); }

void crash_log_write(const char *msg)
{
    if (!msg) {
        return;
    }
    char stamped[288];
    const char *out = msg;
    size_t len = strlen(msg);
    time_t now = time(NULL);
    if (now >= 1577836800 && msg[0] == '[') {
        struct tm tmv;
        gmtime_r(&now, &tmv);
        char ts[24];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
        int n = snprintf(stamped, sizeof(stamped), "[%s] %s", ts, msg);
        if (n > 0) {
            out = stamped;
            len = (size_t)n < sizeof(stamped) ? (size_t)n : sizeof(stamped) - 1;
        }
    }
    tiers_append(out, len);
    if (len == 0 || out[len - 1] != '\n') {
        tiers_append("\n", 1);
    }
}

void crash_log_mark_crash(void)
{
    portENTER_CRITICAL(&s_mux);
    s_rtc.crash_marker = 0xDEADBEEFu;
    portEXIT_CRITICAL(&s_mux);
    nvs_save_rtc();
}

void crash_log_prepare_reboot(void)
{
    nvs_save_rtc();
    portENTER_CRITICAL(&s_mux);
    s_rtc.crash_marker = 0;
    portEXIT_CRITICAL(&s_mux);
}

size_t crash_log_get_ram(char *out, size_t len)
{
    portENTER_CRITICAL(&s_mux);
    size_t n = ring_read(s_ram, RAM_RING_SIZE, s_ram_head, s_ram_count, out, len);
    portEXIT_CRITICAL(&s_mux);
    return n;
}

size_t crash_log_get_rtc(char *out, size_t len)
{
    portENTER_CRITICAL(&s_mux);
    size_t n = ring_read(s_rtc.buf, RTC_RING_SIZE, s_rtc.head, s_rtc.count, out, len);
    portEXIT_CRITICAL(&s_mux);
    return n;
}

size_t crash_log_get_nvs(char *out, size_t len)
{
    if (!out || len == 0) {
        return 0;
    }
    out[0] = '\0';
    nvs_handle_t h;
    if (nvs_open(CRASH_NS, NVS_READONLY, &h) != ESP_OK) {
        return 0;
    }
    size_t blen = 0;
    esp_err_t err = nvs_get_blob(h, "log_data", NULL, &blen);
    if (err == ESP_OK && blen > 0) {
        size_t want = blen < len - 1 ? blen : len - 1;
        nvs_get_blob(h, "log_data", out, &blen);
        out[want] = '\0';
        nvs_close(h);
        return want;
    }
    nvs_close(h);
    return 0;
}

size_t crash_log_get_combined(char *out, size_t len)
{
    if (!out || len == 0) {
        return 0;
    }
    time_t now = time(NULL);
    char ts[24] = "unknown";
    if (now >= 1577836800) {
        struct tm tmv;
        gmtime_r(&now, &tmv);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    }
    int off = snprintf(out, len,
        "===== CRASH LOG DUMP =====\n"
        "Time: %s | Boot: %lu | Uptime: %lld ms | LastBootCrash: %s\n"
        "--- NVS (previous boot) ---\n",
        ts, (unsigned long)s_rtc.boot_count,
        (long long)(esp_timer_get_time() / 1000),
        s_last_crash ? "yes" : "no");
    if (off < 0 || (size_t)off >= len) {
        return len ? len - 1 : 0;
    }
    off += (int)crash_log_get_nvs(out + off, len - off);
    if ((size_t)off < len) {
        off += snprintf(out + off, len - off, "\n--- RTC (since last reboot) ---\n");
    }
    if ((size_t)off < len) {
        off += (int)crash_log_get_rtc(out + off, len - off);
    }
    if ((size_t)off < len) {
        off += snprintf(out + off, len - off, "\n--- RAM (current session) ---\n");
    }
    if ((size_t)off < len) {
        off += (int)crash_log_get_ram(out + off, len - off);
    }
    return (size_t)off < len ? (size_t)off : len - 1;
}

void crash_log_clear_all(void)
{
    portENTER_CRITICAL(&s_mux);
    s_ram_head = 0;
    s_ram_count = 0;
    s_rtc.head = 0;
    s_rtc.count = 0;
    s_rtc.crash_marker = 0;
    portEXIT_CRITICAL(&s_mux);
    panic_text_clear();
    nvs_handle_t h;
    if (nvs_open(CRASH_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, "log_data");
        nvs_erase_key(h, "log_boot");
        nvs_erase_key(h, "log_time");
        nvs_erase_key(h, "log_timestamp");
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Crash logs cleared (all tiers)");
}
