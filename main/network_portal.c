/**
 * @file network_portal.c
 * @brief Captive portal backend. See network_portal.h for scope.
 */

#include "network_portal.h"
#include "app_config.h"
#include "system_crash_log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_task_wdt.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "net_portal";

#define AP_SSID           "AllSky-Display-Setup"
#define AP_IP             "192.168.4.1"
#define PORTAL_SCAN_MAX   20
#define TEST_JOIN_POLLS   20
#define TEST_JOIN_STEP_MS 500

/* Embedded setup page (see CMake EMBED_TXTFILES: main/network_setup_ui.html). */
extern const char network_setup_ui_html_start[] asm("_binary_network_setup_ui_html_start");

static bool s_running;
static bool s_configured;
static httpd_handle_t s_httpd;
static TaskHandle_t s_dns_task;
static volatile bool s_dns_run;
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;

/* Scan cache (own, independent of network_wifi which is not up in this path). */
static wifi_ap_record_t s_scan[PORTAL_SCAN_MAX];
static uint16_t s_scan_count;
static volatile bool s_scan_running;
static volatile bool s_scan_ready;

/* Credential-test state. */
static volatile bool s_test_active;
static volatile bool s_test_got_ip;
static volatile bool s_test_failed;

/* ---- Event handling ------------------------------------------------------- */

static void portal_wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data) {
    switch (id) {
        case WIFI_EVENT_SCAN_DONE: {
            uint16_t n = 0;
            esp_wifi_scan_get_ap_num(&n);
            if (n > PORTAL_SCAN_MAX) n = PORTAL_SCAN_MAX;
            if (n > 0) {
                esp_wifi_scan_get_ap_records(&n, s_scan);
                s_scan_count = n;
            } else {
                esp_wifi_clear_ap_list();
                s_scan_count = 0;
            }
            s_scan_ready = true;
            s_scan_running = false;
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
            if (s_test_active) s_test_failed = true;
            break;
        default:
            break;
    }
}

static void portal_ip_evt(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id == IP_EVENT_STA_GOT_IP && s_test_active) {
        s_test_got_ip = true;
    }
}

/* ---- Wildcard DNS --------------------------------------------------------- */

static void dns_server_task(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket create failed");
        s_dns_task = NULL;
        vTaskDeleteWithCaps(NULL);
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        s_dns_task = NULL;
        vTaskDeleteWithCaps(NULL);
        return;
    }
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint8_t buf[512];
    ip4_addr_t ap_ip;
    ip4addr_aton(AP_IP, &ap_ip);

    while (s_dns_run) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &sl);
        if (n < (int)sizeof(uint8_t) * 12) continue; /* need a header */

        /* Build a response: echo header, mark as answer, copy question, add one
         * A record pointing at the AP IP. */
        buf[2] |= 0x80;             /* QR = response */
        buf[3] = (buf[3] & 0x70) | 0x00; /* RA=0, RCODE=0 */
        buf[6] = 0; buf[7] = 1;     /* ANCOUNT = 1 */
        buf[8] = 0; buf[9] = 0;     /* NSCOUNT = 0 */
        buf[10] = 0; buf[11] = 0;   /* ARCOUNT = 0 */

        int qlen = n; /* question section already ends the received packet */
        if (qlen + 16 > (int)sizeof(buf)) continue;

        uint8_t *p = buf + qlen;
        *p++ = 0xC0; *p++ = 0x0C;   /* name pointer to question */
        *p++ = 0x00; *p++ = 0x01;   /* type A */
        *p++ = 0x00; *p++ = 0x01;   /* class IN */
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3C; /* TTL 60 */
        *p++ = 0x00; *p++ = 0x04;   /* RDLENGTH 4 */
        memcpy(p, &ap_ip.addr, 4);
        p += 4;

        sendto(sock, buf, p - buf, 0, (struct sockaddr *)&src, sl);
    }
    close(sock);
    s_dns_task = NULL;
    vTaskDeleteWithCaps(NULL);
}

/* ---- HTTP helpers --------------------------------------------------------- */

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json) {
    httpd_resp_set_type(req, "application/json");
    if (status) httpd_resp_set_status(req, status);
    return httpd_resp_sendstr(req, json);
}

static void append_escaped(char *dst, size_t cap, const char *src) {
    size_t o = strlen(dst);
    for (size_t i = 0; src[i] && o + 2 < cap; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') dst[o++] = '\\';
        dst[o++] = c;
    }
    dst[o] = '\0';
}

/* application/x-www-form-urlencoded field extraction with percent-decoding. */
static bool form_get(const char *body, const char *key, char *out, size_t out_len) {
    size_t klen = strlen(key);
    const char *p = body;
    out[0] = '\0';
    while (p && *p) {
        const char *eq = strchr(p, '=');
        const char *amp = strchr(p, '&');
        if (!eq) break;
        size_t name_len = eq - p;
        if (name_len == klen && strncmp(p, key, klen) == 0) {
            const char *val = eq + 1;
            const char *end = amp ? amp : (val + strlen(val));
            size_t o = 0;
            for (const char *v = val; v < end && o + 1 < out_len; v++) {
                if (*v == '+') {
                    out[o++] = ' ';
                } else if (*v == '%' && v + 2 < end) {
                    char hex[3] = { v[1], v[2], 0 };
                    out[o++] = (char)strtol(hex, NULL, 16);
                    v += 2;
                } else {
                    out[o++] = *v;
                }
            }
            out[o] = '\0';
            return true;
        }
        p = amp ? amp + 1 : NULL;
    }
    return false;
}

/* ---- HTTP handlers -------------------------------------------------------- */

static esp_err_t root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, network_setup_ui_html_start);
}

static esp_err_t scan_get(httpd_req_t *req) {
    if (!s_scan_running) {
        wifi_scan_config_t sc = { .show_hidden = false };
        s_scan_ready = false;
        s_scan_running = true;
        if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
            s_scan_running = false;
        }
    }
    return send_json(req, NULL, "{\"status\":\"scanning\"}");
}

static int cmp_rssi_desc(const void *a, const void *b) {
    return ((const wifi_ap_record_t *)b)->rssi - ((const wifi_ap_record_t *)a)->rssi;
}

static esp_err_t scan_results_get(httpd_req_t *req) {
    if (s_scan_running || !s_scan_ready) {
        return send_json(req, NULL, "{\"status\":\"scanning\"}");
    }

    /* Post-process: drop empty SSIDs, dedupe keeping strongest, sort strongest first. */
    qsort(s_scan, s_scan_count, sizeof(wifi_ap_record_t), cmp_rssi_desc);

    char *buf = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM);
    if (!buf) return httpd_resp_send_500(req);
    strcpy(buf, "{\"status\":\"complete\",\"networks\":[");

    char seen[PORTAL_SCAN_MAX][33];
    int seen_n = 0, out_n = 0;
    for (int i = 0; i < s_scan_count; i++) {
        const wifi_ap_record_t *r = &s_scan[i];
        if (r->ssid[0] == '\0') continue;
        if (r->primary < 1 || r->primary > 13) continue;
        bool dup = false;
        for (int j = 0; j < seen_n; j++) {
            if (strcmp(seen[j], (const char *)r->ssid) == 0) { dup = true; break; }
        }
        if (dup) continue;
        strlcpy(seen[seen_n++], (const char *)r->ssid, 33);

        char entry[160];
        char esc[100] = "";
        append_escaped(esc, sizeof(esc), (const char *)r->ssid);
        snprintf(entry, sizeof(entry), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"encrypted\":%s}",
                 out_n ? "," : "", esc, r->rssi,
                 r->authmode == WIFI_AUTH_OPEN ? "false" : "true");
        strlcat(buf, entry, 4096);
        out_n++;
    }
    strlcat(buf, "]}", 4096);

    esp_err_t e = send_json(req, NULL, buf);
    heap_caps_free(buf);
    return e;
}

static esp_err_t connect_post(httpd_req_t *req) {
    int total = req->content_len;
    if (total <= 0 || total > 512) return send_json(req, "400 Bad Request",
                                                     "{\"status\":\"error\",\"message\":\"Bad request\"}");
    char body[513];
    int recvd = 0;
    while (recvd < total) {
        int r = httpd_req_recv(req, body + recvd, total - recvd);
        if (r <= 0) return send_json(req, "400 Bad Request",
                                     "{\"status\":\"error\",\"message\":\"Bad request\"}");
        recvd += r;
    }
    body[recvd] = '\0';

    char ssid[33] = "", pwd[64] = "";
    form_get(body, "ssid", ssid, sizeof(ssid));
    if (!form_get(body, "wifi_password", pwd, sizeof(pwd))) {
        form_get(body, "password", pwd, sizeof(pwd));
    }

    if (ssid[0] == '\0') {
        return send_json(req, "400 Bad Request",
                         "{\"status\":\"error\",\"message\":\"SSID is required\"}");
    }

    /* Test the join BEFORE persisting (fix for the legacy always-reboot flow). */
    ESP_LOGI(TAG, "Testing join to '%s'", ssid);
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pwd, sizeof(wc.sta.password));
    esp_wifi_set_config(WIFI_IF_STA, &wc);

    s_test_got_ip = false;
    s_test_failed = false;
    s_test_active = true;
    esp_wifi_disconnect();
    esp_wifi_connect();

    bool ok = false;
    for (int i = 0; i < TEST_JOIN_POLLS; i++) {
        esp_task_wdt_reset();
        if (s_test_got_ip) { ok = true; break; }
        if (s_test_failed && i > 2) break; /* allow a couple of transient retries */
        vTaskDelay(pdMS_TO_TICKS(TEST_JOIN_STEP_MS));
    }
    s_test_active = false;

    if (!ok) {
        ESP_LOGW(TAG, "Test join to '%s' failed", ssid);
        esp_wifi_disconnect();
        return send_json(req, "200 OK",
                         "{\"status\":\"error\",\"message\":\"Could not connect. Check the password and try again.\"}");
    }

    /* Persist and signal a reboot into normal station mode. */
    app_config_set_wifi_ssid(ssid);
    app_config_set_wifi_pwd(pwd);
    app_config_set_wifi_prov(true);
    app_config_save();
    crash_log_prepare_reboot();
    s_configured = true;

    ESP_LOGI(TAG, "Provisioned '%s'; rebooting shortly", ssid);
    char resp[128];
    strcpy(resp, "{\"status\":\"success\",\"message\":\"Connected. The device will restart in a few seconds.\"}");
    return send_json(req, "200 OK", resp);
}

/* Everything else -> the setup page (captive redirect behavior). */
static esp_err_t catchall_get(httpd_req_t *req) {
    return root_get(req);
}

/* ---- Lifecycle ------------------------------------------------------------ */

static esp_err_t start_ap(void) {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    if (!s_ap_netif) s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_ap_netif || !s_sta_netif) return ESP_FAIL;

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, portal_wifi_evt, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, portal_ip_evt, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap = {0};
    strlcpy((char *)ap.ap.ssid, AP_SSID, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(AP_SSID);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

static esp_err_t start_httpd(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_uri_handlers = 8;
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&s_httpd, &cfg) != ESP_OK) return ESP_FAIL;

    const httpd_uri_t routes[] = {
        { .uri = "/scan/results", .method = HTTP_GET,  .handler = scan_results_get },
        { .uri = "/scan",         .method = HTTP_GET,  .handler = scan_get },
        { .uri = "/connect",      .method = HTTP_POST, .handler = connect_post },
        { .uri = "/",             .method = HTTP_GET,  .handler = root_get },
        { .uri = "/*",            .method = HTTP_GET,  .handler = catchall_get },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(s_httpd, &routes[i]);
    }
    return ESP_OK;
}

esp_err_t network_portal_start(void) {
    if (s_running) return ESP_OK;
    s_configured = false;

    esp_err_t err = start_ap();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AP start failed: %s", esp_err_to_name(err));
        return err;
    }

    s_dns_run = true;
    if (xTaskCreateWithCaps(dns_server_task, "portal_dns", 3072, NULL, 4, &s_dns_task,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGW(TAG, "DNS task create failed (captive redirect degraded)");
        s_dns_task = NULL;
    }

    if (start_httpd() != ESP_OK) {
        ESP_LOGE(TAG, "HTTP setup server start failed");
        s_dns_run = false;
        return ESP_FAIL;
    }

    s_running = true;
    ESP_LOGI(TAG, "Captive portal up: SSID '%s' at %s", AP_SSID, AP_IP);
    return ESP_OK;
}

void network_portal_stop(void) {
    if (!s_running) return;
    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    s_dns_run = false; /* task exits on its next 1 s recv timeout */
    esp_wifi_stop();
    s_running = false;
    ESP_LOGI(TAG, "Captive portal stopped");
}

bool network_portal_is_running(void) { return s_running; }
bool network_portal_is_configured(void) { return s_configured; }

const char *network_portal_ap_ssid(void) { return AP_SSID; }
const char *network_portal_ap_ip(void) { return AP_IP; }
