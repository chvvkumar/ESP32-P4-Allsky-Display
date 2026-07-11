/**
 * @file image_download.c
 * @brief Streaming JPEG downloader into a fixed PSRAM buffer.
 */

#include "image_download.h"
#include "network_http_policy.h"

#include <string.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "img_dl";

#define IMG_DL_USER_AGENT        "ESP32-AllSky/1.0"
#define IMG_DL_CONNECT_TIMEOUT   8000      /* ms, connect + request setup */
#define IMG_DL_TOTAL_TIMEOUT_US  (90LL * 1000000) /* 90 s whole-download deadline */
#define IMG_DL_STALL_US          (5LL * 1000000)  /* no data for 5 s -> stall */
#define IMG_DL_CHUNK_ABORT_US    (16LL * 1000000) /* single read over 16 s -> abort */
#define IMG_DL_CHUNK_WARN_US     (8LL * 1000000)  /* single read over 8 s -> warn */
#define IMG_DL_READ_CHUNK        8192      /* bytes per read call */
#define IMG_DL_WDT_INTERVAL_US   (50LL * 1000)    /* feed watchdog at least every 50 ms */
#define IMG_DL_MAX_REDIRECTS     5
#define IMG_DL_MIN_VALID_BYTES   1024

const char *image_dl_status_str(image_dl_status_t s)
{
    switch (s) {
        case IMG_DL_OK:            return "ok";
        case IMG_DL_ERR_ARG:       return "bad-arg";
        case IMG_DL_ERR_CONNECT:   return "connect-failed";
        case IMG_DL_ERR_STATUS:    return "bad-status";
        case IMG_DL_ERR_TOO_LARGE: return "too-large";
        case IMG_DL_ERR_EMPTY:     return "empty";
        case IMG_DL_ERR_TIMEOUT:   return "timeout";
        case IMG_DL_ERR_CONN_LOST: return "conn-lost";
        case IMG_DL_ERR_INCOMPLETE:return "incomplete";
        case IMG_DL_ERR_NOT_JPEG:  return "not-jpeg";
        default:                   return "unknown";
    }
}

static void feed(const image_dl_req_t *req)
{
    if (req->watchdog_feed) {
        req->watchdog_feed();
    }
}

/* Open the client and follow the redirect chain manually (streaming open/read
 * does not auto-follow). Returns ESP_OK with status + content_length filled. */
static esp_err_t open_follow(esp_http_client_handle_t client, int *status_out,
                             int64_t *content_length_out)
{
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        return err;
    }
    int64_t content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    int redirects = 0;
    while (http_status_is_redirect(status) && redirects < IMG_DL_MAX_REDIRECTS) {
        err = esp_http_client_set_redirection(client);
        if (err != ESP_OK) {
            break;
        }
        esp_http_client_close(client);
        err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            return err;
        }
        content_length = esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        redirects++;
    }

    *status_out = status;
    *content_length_out = content_length;
    return ESP_OK;
}

static image_dl_status_t validate_body(const uint8_t *buf, size_t len)
{
    if (len < IMG_DL_MIN_VALID_BYTES) {
        return IMG_DL_ERR_EMPTY;
    }
    /* PNG magic: 89 50 4E 47 */
    if (buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47) {
        ESP_LOGW(TAG, "PNG not supported, JPEG only");
        return IMG_DL_ERR_NOT_JPEG;
    }
    /* JPEG SOI: FF D8 */
    if (buf[0] != 0xFF || buf[1] != 0xD8) {
        return IMG_DL_ERR_NOT_JPEG;
    }
    return IMG_DL_OK;
}

image_dl_status_t image_download(const image_dl_req_t *req, size_t *out_len)
{
    if (out_len) {
        *out_len = 0;
    }
    if (!req || !req->url || !req->buf || req->buf_cap < IMG_DL_MIN_VALID_BYTES) {
        return IMG_DL_ERR_ARG;
    }

    bool is_https = strncmp(req->url, "https://", 8) == 0;
    esp_http_client_config_t cfg = {
        .url = req->url,
        .timeout_ms = IMG_DL_CONNECT_TIMEOUT,
        .keep_alive_enable = false,
        .crt_bundle_attach = (is_https && !req->skip_tls_verify) ? esp_crt_bundle_attach : NULL,
        .skip_cert_common_name_check = req->skip_tls_verify,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return IMG_DL_ERR_CONNECT;
    }

    esp_http_client_set_header(client, "User-Agent", IMG_DL_USER_AGENT);
    esp_http_client_set_header(client, "Connection", "close");
    esp_http_client_set_header(client, "Cache-Control", "no-cache");

    int status = 0;
    int64_t content_length = 0;
    esp_err_t err = open_follow(client, &status, &content_length);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "open failed for %s: %s", req->url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return IMG_DL_ERR_CONNECT;
    }

    if (status != 200) {
        ESP_LOGW(TAG, "HTTP %d for %s", status, req->url);
        esp_http_client_cleanup(client);
        return IMG_DL_ERR_STATUS;
    }

    if (content_length == 0) {
        ESP_LOGW(TAG, "Content-Length 0 for %s", req->url);
        esp_http_client_cleanup(client);
        return IMG_DL_ERR_EMPTY;
    }
    /* Declared and too big to hold (need room for the whole body). */
    if (content_length > 0 && (size_t)content_length >= req->buf_cap) {
        ESP_LOGW(TAG, "image too large: %lld bytes (cap %u)",
                 (long long)content_length, (unsigned)req->buf_cap);
        esp_http_client_cleanup(client);
        return IMG_DL_ERR_TOO_LARGE;
    }

    size_t total = 0;
    int64_t start_us = esp_timer_get_time();
    int64_t last_data_us = start_us;
    int64_t last_feed_us = start_us;
    int64_t last_log_us = start_us;
    image_dl_status_t result = IMG_DL_OK;

    for (;;) {
        int64_t now = esp_timer_get_time();

        if (now - start_us > IMG_DL_TOTAL_TIMEOUT_US) {
            ESP_LOGW(TAG, "total download timeout (%u bytes) for %s",
                     (unsigned)total, req->url);
            result = IMG_DL_ERR_TIMEOUT;
            break;
        }
        if (now - last_data_us > IMG_DL_STALL_US) {
            ESP_LOGW(TAG, "download stalled (no data 5s, %u bytes) for %s",
                     (unsigned)total, req->url);
            result = IMG_DL_ERR_TIMEOUT;
            break;
        }
        if (now - last_feed_us > IMG_DL_WDT_INTERVAL_US) {
            feed(req);
            last_feed_us = now;
        }

        if (total >= req->buf_cap) {
            ESP_LOGW(TAG, "download buffer overflow for %s", req->url);
            result = IMG_DL_ERR_TOO_LARGE;
            break;
        }

        size_t want = req->buf_cap - total;
        if (want > IMG_DL_READ_CHUNK) {
            want = IMG_DL_READ_CHUNK;
        }

        int64_t read_start = esp_timer_get_time();
        int n = esp_http_client_read(client, (char *)req->buf + total, (int)want);
        int64_t read_dur = esp_timer_get_time() - read_start;

        if (read_dur > IMG_DL_CHUNK_ABORT_US) {
            ESP_LOGW(TAG, "single chunk read exceeded 16s for %s", req->url);
            result = IMG_DL_ERR_TIMEOUT;
            break;
        }
        if (read_dur > IMG_DL_CHUNK_WARN_US) {
            ESP_LOGW(TAG, "slow chunk read (%lld ms) for %s",
                     (long long)(read_dur / 1000), req->url);
        }

        if (n < 0) {
            ESP_LOGW(TAG, "read error (%u bytes) for %s", (unsigned)total, req->url);
            result = IMG_DL_ERR_CONN_LOST;
            break;
        }
        if (n == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                break; /* clean end of body */
            }
            if (esp_http_client_is_chunked_response(client) || content_length < 0) {
                /* Unknown-length stream: a zero read with the connection closed
                 * is a clean finish. */
                break;
            }
            /* Declared length but connection closed early is caught below by the
             * incomplete check; loop again to let stall/timeout guards fire. */
            now = esp_timer_get_time();
            if (now - last_data_us > IMG_DL_STALL_US) {
                result = IMG_DL_ERR_CONN_LOST;
                break;
            }
            continue;
        }

        total += (size_t)n;
        last_data_us = esp_timer_get_time();

        if (last_data_us - last_log_us >= 1000000) {
            float kbps = (float)total / 1024.0f /
                         ((float)(last_data_us - start_us) / 1000000.0f);
            if (content_length > 0) {
                ESP_LOGI(TAG, "download %u/%lld bytes (%.0f%%, %.1f KB/s)",
                         (unsigned)total, (long long)content_length,
                         100.0f * (float)total / (float)content_length, kbps);
            } else {
                ESP_LOGI(TAG, "download %u bytes (%.1f KB/s)", (unsigned)total, kbps);
            }
            last_log_us = last_data_us;
        }
    }

    esp_http_client_cleanup(client);

    if (result != IMG_DL_OK) {
        return result;
    }

    if (content_length > 0 && total < (size_t)content_length) {
        ESP_LOGW(TAG, "incomplete: %u/%lld bytes for %s",
                 (unsigned)total, (long long)content_length, req->url);
        return IMG_DL_ERR_INCOMPLETE;
    }

    image_dl_status_t v = validate_body(req->buf, total);
    if (v != IMG_DL_OK) {
        return v;
    }

    if (out_len) {
        *out_len = total;
    }
    ESP_LOGI(TAG, "downloaded %u bytes from %s", (unsigned)total, req->url);
    return IMG_DL_OK;
}
