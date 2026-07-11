/**
 * @file network_http.c
 * @brief Shared JSON/text HTTP GET fetcher. See network_http.h for scope.
 */

#include "network_http.h"
#include "network_http_policy.h"

#include <string.h>
#include <strings.h>

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "net_http";

struct http_fetch_conn {
    esp_http_client_handle_t client; /* NULL until first successful fetch */
};

/**
 * Clear the caller's capture buffer (if provided). Called before every
 * response-header fetch -- including each redirect re-open -- so that only the
 * FINAL response's header value survives.
 */
static void capture_reset(const http_fetch_opts_t *opts) {
    if (opts->capture_header_out && opts->capture_header_out_len > 0) {
        opts->capture_header_out[0] = '\0';
    }
}

/**
 * esp_http_client event callback: arbitrary response headers are only
 * observable via HTTP_EVENT_ON_HEADER. evt->user_data is re-pointed per request
 * so a keep-alive handle can serve different opts across requests.
 */
static esp_err_t header_capture_event_cb(esp_http_client_event_t *evt) {
    if (evt->event_id != HTTP_EVENT_ON_HEADER) return ESP_OK;

    const http_fetch_opts_t *opts = (const http_fetch_opts_t *)evt->user_data;
    if (!opts || !opts->capture_header ||
        !opts->capture_header_out || opts->capture_header_out_len == 0) {
        return ESP_OK;
    }
    if (!evt->header_key || !evt->header_value) return ESP_OK;
    if (strcasecmp(evt->header_key, opts->capture_header) != 0) return ESP_OK;

    size_t n = strnlen(evt->header_value, opts->capture_header_out_len - 1);
    memcpy(opts->capture_header_out, evt->header_value, n);
    opts->capture_header_out[n] = '\0';
    return ESP_OK;
}

/** Fill in defaults for any unset (zero/negative) field. */
static void normalize_opts(http_fetch_opts_t *o) {
    if (o->timeout_ms <= 0) o->timeout_ms = 8000;
    if (o->max_attempts <= 0) o->max_attempts = 1;
    if (o->retry_delay_ms <= 0) o->retry_delay_ms = 500;
    if (o->max_response_bytes == 0) o->max_response_bytes = 65536;
}

/** Allocate a fresh esp_http_client for @p url per @p opts. */
static esp_http_client_handle_t make_client(const char *url,
                                            const http_fetch_opts_t *opts,
                                            bool keep_alive) {
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = opts->timeout_ms,
        .keep_alive_enable = keep_alive,
        .crt_bundle_attach = opts->use_tls_bundle ? esp_crt_bundle_attach : NULL,
        .event_handler = header_capture_event_cb,
    };
    return esp_http_client_init(&cfg);
}

/** Apply the optional headers from @p opts to @p client. */
static void apply_headers(esp_http_client_handle_t client, const http_fetch_opts_t *opts) {
    if (opts->host_header) {
        esp_http_client_set_header(client, "Host", opts->host_header);
    }
    if (opts->bearer_token) {
        char hdr[1200];
        int n = snprintf(hdr, sizeof(hdr), "Bearer %s", opts->bearer_token);
        if (n > 0 && n < (int)sizeof(hdr)) {
            esp_http_client_set_header(client, "Authorization", hdr);
        }
    }
    if (opts->user_agent) {
        esp_http_client_set_header(client, "User-Agent", opts->user_agent);
    }
    if (opts->accept) {
        esp_http_client_set_header(client, "Accept", opts->accept);
    }
}

/**
 * Open @p client, fetch headers, and follow any redirect chain (streaming
 * open()/read() does not auto-follow -- must be done manually per hop).
 */
static esp_err_t open_and_follow_redirects(esp_http_client_handle_t client,
                                           const http_fetch_opts_t *opts,
                                           int *status_out, int *content_length_out,
                                           bool *opened_out, int64_t *connect_us_out,
                                           int64_t *headers_us_out) {
    capture_reset(opts);
    int64_t t0 = esp_timer_get_time();
    esp_err_t err = esp_http_client_open(client, 0);
    if (connect_us_out) *connect_us_out += esp_timer_get_time() - t0;
    if (err != ESP_OK) return err;
    if (opened_out) *opened_out = true;

    int64_t t1 = esp_timer_get_time();
    int content_length = esp_http_client_fetch_headers(client);
    if (headers_us_out) *headers_us_out += esp_timer_get_time() - t1;
    int status = esp_http_client_get_status_code(client);

    int redirects = 0;
    while (http_status_is_redirect(status) && redirects < opts->max_redirects) {
        err = esp_http_client_set_redirection(client);
        if (err != ESP_OK) break;

        esp_http_client_close(client);
        capture_reset(opts);
        t0 = esp_timer_get_time();
        err = esp_http_client_open(client, 0);
        if (connect_us_out) *connect_us_out += esp_timer_get_time() - t0;
        if (err != ESP_OK) return err;

        t1 = esp_timer_get_time();
        content_length = esp_http_client_fetch_headers(client);
        if (headers_us_out) *headers_us_out += esp_timer_get_time() - t1;
        status = esp_http_client_get_status_code(client);
        redirects++;
    }

    *status_out = status;
    *content_length_out = content_length;
    return ESP_OK;
}

/** Close (keep-alive) or cleanup @p client depending on whether @p conn is set. */
static void finish_client(esp_http_client_handle_t client, http_fetch_conn_t *conn) {
    if (conn) {
        esp_http_client_set_user_data(client, NULL);
        esp_http_client_close(client);
        conn->client = client;
    } else {
        esp_http_client_cleanup(client);
    }
}

/** Perform a single GET fetch attempt. */
static esp_err_t attempt_once(const char *url, const http_fetch_opts_t *opts,
                              char **out_body, size_t *out_len, bool *retryable,
                              http_fetch_attempt_info_t *info) {
    *retryable = true;
    http_fetch_conn_t *conn = opts->conn;
    bool reused = (conn && conn->client != NULL);

    esp_http_client_handle_t client;
    if (reused) {
        client = conn->client;
        esp_http_client_set_url(client, url);
    } else {
        client = make_client(url, opts, conn != NULL);
        if (!client) return ESP_ERR_NO_MEM;
    }
    apply_headers(client, opts);
    esp_http_client_set_user_data(client, (void *)opts);

    int status = 0;
    int content_length = 0;
    esp_err_t err = open_and_follow_redirects(client, opts, &status, &content_length,
                                              &info->ever_connected, &info->connect_us,
                                              &info->headers_us);

    if ((err != ESP_OK || status == -1) && reused) {
        ESP_LOGD(TAG, "Stale keep-alive for %s -- reconnecting", url);
        esp_http_client_cleanup(client);
        conn->client = NULL;

        client = make_client(url, opts, true);
        if (!client) return ESP_ERR_NO_MEM;
        apply_headers(client, opts);
        esp_http_client_set_user_data(client, (void *)opts);
        err = open_and_follow_redirects(client, opts, &status, &content_length,
                                        &info->ever_connected, &info->connect_us,
                                        &info->headers_us);
    }

    info->status = status;

    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP %d for %s", status, url);
        finish_client(client, conn);
        *retryable = (status >= 500);
        return ESP_FAIL;
    }

    size_t cap = opts->max_response_bytes;

    if (content_length > 0 && (size_t)content_length + 1 > cap) {
        ESP_LOGW(TAG, "response too large (%d bytes, cap %u) for %s",
                 content_length, (unsigned)cap, url);
        finish_client(client, conn);
        *retryable = false;
        return ESP_ERR_INVALID_SIZE;
    }

    size_t bufsize = http_buf_initial(content_length, cap);
    if (bufsize == 0) {
        finish_client(client, conn);
        *retryable = false;
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = heap_caps_malloc(bufsize, MALLOC_CAP_SPIRAM);
    if (!buf) {
        finish_client(client, conn);
        *retryable = false;
        return ESP_ERR_NO_MEM;
    }

    size_t total = 0;
    bool truncated = false;
    int64_t body_start_us = esp_timer_get_time();
    while (total + 1 < bufsize) {
        int n = esp_http_client_read(client, buf + total, (int)(bufsize - 1 - total));
        if (n <= 0) break;
        total += (size_t)n;

        if (total + 1 >= bufsize) {
            size_t grown = http_buf_grow(bufsize, cap);
            if (grown == 0) {
                truncated = (content_length <= 0);
                break;
            }
            char *nb = heap_caps_realloc(buf, grown, MALLOC_CAP_SPIRAM);
            if (!nb) {
                info->body_us = esp_timer_get_time() - body_start_us;
                heap_caps_free(buf);
                finish_client(client, conn);
                *retryable = false;
                return ESP_ERR_NO_MEM;
            }
            buf = nb;
            bufsize = grown;
        }
    }
    info->body_us = esp_timer_get_time() - body_start_us;
    buf[total] = '\0';

    if (content_length > 0 && total < (size_t)content_length) {
        ESP_LOGW(TAG, "Partial HTTP read: %u/%d bytes for %s",
                 (unsigned)total, content_length, url);
        heap_caps_free(buf);
        finish_client(client, conn);
        return ESP_FAIL;
    }

    if (truncated) {
        ESP_LOGW(TAG, "Response truncated at cap (%u bytes) for %s", (unsigned)cap, url);
    }

    finish_client(client, conn);
    *out_body = buf;
    *out_len = total;
    info->ok = true;
    return ESP_OK;
}

esp_err_t http_fetch_text(const char *url, const http_fetch_opts_t *opts_in,
                          char **out_body, size_t *out_len) {
    if (!url || !out_body || !out_len) return ESP_ERR_INVALID_ARG;

    http_fetch_opts_t opts = opts_in ? *opts_in : (http_fetch_opts_t){0};
    normalize_opts(&opts);

    esp_err_t last_err = ESP_FAIL;
    for (int attempt = 0; attempt < opts.max_attempts; attempt++) {
        if (attempt > 0) {
            vTaskDelay(pdMS_TO_TICKS(opts.retry_delay_ms));
        }

        bool retryable = true;
        http_fetch_attempt_info_t info = { .attempt_index = attempt };
        esp_err_t err = attempt_once(url, &opts, out_body, out_len, &retryable, &info);
        if (opts.on_attempt) opts.on_attempt(&info, opts.hook_ctx);
        if (opts.status_out && info.status != 0) *opts.status_out = info.status;
        if (err == ESP_OK) return ESP_OK;

        last_err = err;
        if (!retryable) break;
        if (!http_should_retry(attempt, opts.max_attempts)) break;
    }

    ESP_LOGW(TAG, "%s unreachable (%s)", url, esp_err_to_name(last_err));
    return last_err;
}

cJSON *http_fetch_json(const char *url, const http_fetch_opts_t *opts) {
    char *body = NULL;
    size_t len = 0;
    if (http_fetch_text(url, opts, &body, &len) != ESP_OK) {
        return NULL;
    }

    cJSON *json = cJSON_Parse(body);
    heap_caps_free(body);
    if (!json) {
        ESP_LOGW(TAG, "JSON parse failed for %s", url);
    }
    return json;
}

esp_err_t http_fetch_head(const char *url, const http_fetch_opts_t *opts_in) {
    if (!url) return ESP_ERR_INVALID_ARG;

    http_fetch_opts_t opts = opts_in ? *opts_in : (http_fetch_opts_t){0};
    normalize_opts(&opts);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = opts.timeout_ms,
        .method = HTTP_METHOD_HEAD,
        .keep_alive_enable = false,
        .crt_bundle_attach = opts.use_tls_bundle ? esp_crt_bundle_attach : NULL,
        .event_handler = header_capture_event_cb,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    apply_headers(client, &opts);
    esp_http_client_set_user_data(client, (void *)&opts);

    int status = 0, content_length = 0;
    bool opened = false;
    esp_err_t err = open_and_follow_redirects(client, &opts, &status, &content_length,
                                              &opened, NULL, NULL);
    if (opts.status_out && status != 0) *opts.status_out = status;

    esp_http_client_cleanup(client);

    if (err != ESP_OK) return err;
    if (status < 200 || status >= 400) return ESP_FAIL;
    return ESP_OK;
}

http_fetch_conn_t *http_fetch_conn_create(void) {
    return heap_caps_calloc(1, sizeof(http_fetch_conn_t), MALLOC_CAP_SPIRAM);
}

void http_fetch_conn_destroy(http_fetch_conn_t *conn) {
    if (!conn) return;
    if (conn->client) {
        esp_http_client_cleanup(conn->client);
    }
    heap_caps_free(conn);
}
