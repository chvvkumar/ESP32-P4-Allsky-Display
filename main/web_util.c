#include "web_util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "web_util";

int web_read_body(httpd_req_t *req, char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) return -1;
    size_t total = req->content_len;
    if (total >= buf_len) {
        ESP_LOGW(TAG, "body too large: %u >= %u", (unsigned)total, (unsigned)buf_len);
        return -1;
    }
    size_t received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return -1;
        }
        received += r;
    }
    buf[received] = '\0';
    return (int)received;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* URL-decode src into dst (bounded). '+' becomes space, %XX decoded. */
static void url_decode(char *dst, size_t dst_len, const char *src)
{
    size_t o = 0;
    for (const char *p = src; *p && o + 1 < dst_len; p++) {
        if (*p == '+') {
            dst[o++] = ' ';
        } else if (*p == '%' && p[1] && p[2]) {
            int hi = hexval(p[1]), lo = hexval(p[2]);
            if (hi >= 0 && lo >= 0) {
                dst[o++] = (char)((hi << 4) | lo);
                p += 2;
            } else {
                dst[o++] = *p;
            }
        } else {
            dst[o++] = *p;
        }
    }
    dst[o] = '\0';
}

bool web_form_get(const char *body, const char *key, char *out, size_t out_len)
{
    if (!body || !key || !out || out_len == 0) return false;
    out[0] = '\0';
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        /* match key= at a field boundary */
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v = p + klen + 1;
            const char *end = strchr(v, '&');
            size_t rawlen = end ? (size_t)(end - v) : strlen(v);
            char raw[512];
            if (rawlen >= sizeof(raw)) rawlen = sizeof(raw) - 1;
            memcpy(raw, v, rawlen);
            raw[rawlen] = '\0';
            url_decode(out, out_len, raw);
            return true;
        }
        const char *amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return false;
}

bool web_form_has(const char *body, const char *key)
{
    if (!body || !key) return false;
    size_t klen = strlen(key);
    const char *p = body;
    while (p && *p) {
        if (strncmp(p, key, klen) == 0 && (p[klen] == '=' )) return true;
        const char *amp = strchr(p, '&');
        if (!amp) break;
        p = amp + 1;
    }
    return false;
}

bool web_query_get(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    if (!out || out_len == 0) return false;
    out[0] = '\0';
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) return false;
    char *q = heap_caps_malloc(qlen + 1, MALLOC_CAP_SPIRAM);
    if (!q) return false;
    bool ok = false;
    if (httpd_req_get_url_query_str(req, q, qlen + 1) == ESP_OK) {
        char raw[256];
        if (httpd_query_key_value(q, key, raw, sizeof(raw)) == ESP_OK) {
            url_decode(out, out_len, raw);
            ok = true;
        }
    }
    free(q);
    return ok;
}

esp_err_t web_send_status(httpd_req_t *req, int http_code, const char *status_str,
                          const char *message, const char *extra_json)
{
    char status_line[32];
    const char *reason =
        http_code == 200 ? "OK" :
        http_code == 400 ? "Bad Request" :
        http_code == 401 ? "Unauthorized" :
        http_code == 404 ? "Not Found" :
        http_code == 409 ? "Conflict" :
        http_code == 429 ? "Too Many Requests" :
        http_code == 500 ? "Internal Server Error" :
        http_code == 502 ? "Bad Gateway" :
        http_code == 503 ? "Service Unavailable" : "OK";
    snprintf(status_line, sizeof(status_line), "%d %s", http_code, reason);
    httpd_resp_set_status(req, status_line);
    httpd_resp_set_type(req, "application/json");

    char esc[256];
    esc[0] = '\0';
    if (message) web_json_escape(esc, sizeof(esc), message);

    char body[640];
    int n = snprintf(body, sizeof(body), "{\"status\":\"%s\",\"message\":\"%s\"%s%s}",
                     status_str ? status_str : "success", esc,
                     (extra_json && extra_json[0]) ? "," : "",
                     (extra_json && extra_json[0]) ? extra_json : "");
    if (n < 0) return ESP_FAIL;
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_send_json(httpd_req_t *req, int http_code, const char *json)
{
    char status_line[32];
    const char *reason = http_code == 200 ? "OK" :
                         http_code == 400 ? "Bad Request" :
                         http_code == 500 ? "Internal Server Error" : "OK";
    snprintf(status_line, sizeof(status_line), "%d %s", http_code, reason);
    httpd_resp_set_status(req, status_line);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_send_400(httpd_req_t *req, const char *message)
{
    return web_send_status(req, 400, "error", message, NULL);
}

esp_err_t web_send_ok(httpd_req_t *req, const char *message)
{
    return web_send_status(req, 200, "success", message, NULL);
}

void web_html_escape(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) return;
    size_t o = strlen(dst);
    for (const char *p = src ? src : ""; *p && o + 6 < dst_len; p++) {
        const char *rep = NULL;
        switch (*p) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: dst[o++] = *p; continue;
        }
        size_t rl = strlen(rep);
        if (o + rl >= dst_len) break;
        memcpy(dst + o, rep, rl);
        o += rl;
    }
    dst[o] = '\0';
}

void web_json_escape(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) return;
    size_t o = strlen(dst);
    for (const char *p = src ? src : ""; *p && o + 7 < dst_len; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            dst[o++] = '\\'; dst[o++] = c;
        } else if (c == '\n') { dst[o++] = '\\'; dst[o++] = 'n'; }
        else if (c == '\r') { dst[o++] = '\\'; dst[o++] = 'r'; }
        else if (c == '\t') { dst[o++] = '\\'; dst[o++] = 't'; }
        else if (c == '\b') { dst[o++] = '\\'; dst[o++] = 'b'; }
        else if (c == '\f') { dst[o++] = '\\'; dst[o++] = 'f'; }
        else if (c < 0x20) {
            o += snprintf(dst + o, dst_len - o, "\\u%04x", c);
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = '\0';
}
