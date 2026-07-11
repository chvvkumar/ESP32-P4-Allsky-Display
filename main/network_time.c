/**
 * @file network_time.c
 * @brief Two-stage time sync: SNTP primary, HTTP-Date fallback. See header.
 */

#include "network_time.h"
#include "network_timeparse.h"
#include "app_config.h"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_task_wdt.h"

static const char *TAG = "net_time";

/* Cadence constants (network.md 5). */
#define FIRST_HTTP_DELAY_MS    8000
#define HTTP_INVALID_RETRY_MS  20000
#define HTTP_RESYNC_MS         21600000  /* 6 h drift correction */
#define SNTP_POLL_MS           500
#define SNTP_MAX_POLLS         10
#define HEAD_TIMEOUT_MS        3000
#define EPOCH_SANITY           1577836800LL /* 2020-01-01Z */

static bool s_sntp_started;
static bool s_sntp_active;
static int  s_sntp_polls;
static int64_t s_sntp_next_poll_ms;
static int64_t s_http_next_ms;
static bool s_http_scheduled;
static char s_ntp_server[64];

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

/* Case-insensitive substring test (newlib lacks strcasestr). */
static bool contains_ci(const char *hay, const char *needle) {
    if (!hay[0]) return false;
    size_t nl = strlen(needle);
    for (const char *p = hay; *p; p++) {
        if (strncasecmp(p, needle, nl) == 0) return true;
    }
    return false;
}

static void feed_wdt(void) {
    /* Best effort: errors if the caller task is not subscribed. */
    esp_task_wdt_reset();
}

esp_err_t network_time_init(void) {
    char tz[64];
    app_config_get_timezone(tz, sizeof(tz));
    setenv("TZ", tz, 1);
    tzset();
    return ESP_OK;
}

bool network_time_is_valid(void) {
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return (tm.tm_year + 1900) > 2020;
}

bool network_time_sntp_in_progress(void) {
    return s_sntp_active;
}

static void start_sntp(void) {
    if (!app_config_get_ntp_enabled()) {
        s_sntp_active = false;
        return;
    }
    app_config_get_ntp_server(s_ntp_server, sizeof(s_ntp_server));

    if (!s_sntp_started) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, s_ntp_server);
        esp_sntp_init();
        s_sntp_started = true;
    } else {
        esp_sntp_setservername(0, s_ntp_server);
        esp_sntp_restart();
    }
    s_sntp_active = true;
    s_sntp_polls = 0;
    s_sntp_next_poll_ms = now_ms() + SNTP_POLL_MS;
}

void network_time_on_connected(void) {
    start_sntp();
    s_http_next_ms = now_ms() + FIRST_HTTP_DELAY_MS;
    s_http_scheduled = true;
}

void network_time_resync(void) {
    /* Re-apply TZ in case it changed, then force both paths. */
    char tz[64];
    app_config_get_timezone(tz, sizeof(tz));
    setenv("TZ", tz, 1);
    tzset();
    start_sntp();
    s_http_next_ms = now_ms();
    s_http_scheduled = true;
}

/* HEAD @p url capturing Date/Age/X-Cache. Returns ESP_OK if a response with a
 * Date header arrived. */
typedef struct {
    char date[48];
    char age[16];
    char xcache[64];
} date_headers_t;

static esp_err_t head_evt(esp_http_client_event_t *evt) {
    if (evt->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;
    date_headers_t *h = (date_headers_t *)evt->user_data;
    if (!h || !evt->header_key || !evt->header_value) return ESP_OK;
    if (strcasecmp(evt->header_key, "Date") == 0) {
        strlcpy(h->date, evt->header_value, sizeof(h->date));
    } else if (strcasecmp(evt->header_key, "Age") == 0) {
        strlcpy(h->age, evt->header_value, sizeof(h->age));
    } else if (strcasecmp(evt->header_key, "X-Cache") == 0) {
        strlcpy(h->xcache, evt->header_value, sizeof(h->xcache));
    }
    return ESP_OK;
}

static esp_err_t date_head_fetch(const char *url, date_headers_t *h) {
    memset(h, 0, sizeof(*h));
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_HEAD,
        .timeout_ms = HEAD_TIMEOUT_MS,
        .keep_alive_enable = false,
        .event_handler = head_evt,
        .user_data = h,
        .crt_bundle_attach = esp_crt_bundle_attach, /* validates https; ignored for http */
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_ERR_NO_MEM;
    esp_err_t err = esp_http_client_perform(c);
    esp_http_client_cleanup(c);
    return err;
}

/* One HTTP-Date sync attempt against the configured image server. */
static void http_date_attempt(void) {
    char url[256];
    app_config_current_image_url(url, sizeof(url));
    if (url[0] == '\0') return;

    feed_wdt();
    date_headers_t h;
    esp_err_t err = date_head_fetch(url, &h);
    feed_wdt();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP-Date HEAD failed for %s: %s", url, esp_err_to_name(err));
        return;
    }
    if (strnlen(h.date, sizeof(h.date)) < 20) {
        ESP_LOGW(TAG, "HTTP-Date header too short/absent from %s", url);
        return;
    }
    /* Cache rejection (5.2.4). */
    bool cache_hit = contains_ci(h.xcache, "hit");
    int age = (h.age[0]) ? atoi(h.age) : 0;
    if (cache_hit || (h.age[0] && age > 60)) {
        ESP_LOGW(TAG, "Rejecting cached Date from %s (X-Cache='%s' Age='%s')",
                 url, h.xcache, h.age);
        return;
    }

    time_t epoch = time_parse_rfc1123(h.date);
    if (epoch < EPOCH_SANITY) {
        ESP_LOGW(TAG, "HTTP-Date unparseable/insane: '%s'", h.date);
        return;
    }

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "Clock set from HTTP Date (%s): %s", url, h.date);
}

void network_time_update(void) {
    int64_t t = now_ms();

    /* SNTP validity polling. */
    if (s_sntp_active && t >= s_sntp_next_poll_ms) {
        if (network_time_is_valid()) {
            time_t now = time(NULL);
            struct tm tm;
            localtime_r(&now, &tm);
            char b[32];
            strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tm);
            ESP_LOGI(TAG, "SNTP synced: %s", b);
            s_sntp_active = false;
        } else if (++s_sntp_polls >= SNTP_MAX_POLLS) {
            ESP_LOGW(TAG, "SNTP gave up after %d polls; HTTP-Date is authoritative",
                     SNTP_MAX_POLLS);
            s_sntp_active = false;
        } else {
            s_sntp_next_poll_ms = t + SNTP_POLL_MS;
        }
    }

    /* HTTP-Date sync (rollover-safe due comparison). */
    if (s_http_scheduled && (t - s_http_next_ms) >= 0) {
        http_date_attempt();
        int64_t nxt = network_time_is_valid() ? HTTP_RESYNC_MS : HTTP_INVALID_RETRY_MS;
        s_http_next_ms = now_ms() + nxt;
    }
}
