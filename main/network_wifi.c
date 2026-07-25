/**
 * @file network_wifi.c
 * @brief Station-mode WiFi manager. See network_wifi.h for scope.
 */

#include "network_wifi.h"
#include "network_time.h"
#include "app_config.h"
#include "system_health.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "net_wifi";

/* Timing constants (network.md "Compile-time constants"). */
#define CONNECT_TIMEOUT_MS   12000
#define BACKOFF_MIN_MS        2000
#define BACKOFF_MAX_MS       60000
#define ROAM_SCAN_INTERVAL   60000
/* Skip the periodic roam scan while the current link is stronger than this
 * (dBm). A strong link cannot be improved enough to justify a full scan, so
 * gating here avoids thousands of wasted scans over a long stable session. */
#define ROAM_SCAN_RSSI_GATE    (-65)
#define ROAM_RSSI_MARGIN_DB      8
#define ROAM_TIMEOUT_MS      12000
#define ROAM_DISC_WAIT_MS     3000
#define SCAN_RATE_LIMIT_MS   10000
#define SCAN_MAX_RECORDS        30
#define SCAN_UI_WAIT_MS       6000

typedef enum {
    NET_IDLE = 0,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_ROAMING,
} net_state_t;

/* ---- Module state (owned by the update tick unless noted volatile) -------- */

static esp_netif_t *s_sta_netif;
static EventGroupHandle_t s_event_group;
static bool s_inited;
static bool s_started;                 /* radio started */

static net_state_t s_state = NET_IDLE;
static uint32_t s_backoff_ms = BACKOFF_MIN_MS;
static int64_t s_attempt_start_ms;
static int64_t s_last_attempt_ms = -1000000;
static bool s_prev_connected;          /* for edge logging */
static bool s_ssid_err_logged;

/* Latched by event handlers, read by the tick. */
static volatile bool s_link_up;        /* STA associated */
static volatile bool s_got_ip;         /* DHCP lease held */
static volatile bool s_disc_pending;   /* a disconnect event arrived */
static volatile int  s_disc_reason;

/* Cached connection facts (written under got-ip event). */
static char s_ip[16] = "0.0.0.0";
static char s_gw[16] = "0.0.0.0";
static char s_dns[16] = "0.0.0.0";
static char s_mac[18];
static char s_hostname[64];
static char s_ssid[33];
static char s_pwd[64];

/* Roaming. */
static int64_t s_last_roam_scan_ms = -1000000;
static int64_t s_roam_start_ms;
static uint8_t s_roam_bssid[6];
static uint8_t s_roam_channel;
static bool s_roam_join_issued;

/* Scan cache, shared between the event handler, the tick, and the web task. */
static SemaphoreHandle_t s_scan_mutex;
static wifi_ap_record_t *s_scan_cache; /* PSRAM, SCAN_MAX_RECORDS */
static uint16_t s_scan_count;
static volatile bool s_scan_running;
static bool s_scan_ready;
static bool s_scan_is_roam;
static int64_t s_last_scan_start_ms = -1000000;

static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

/* ---- Hostname derivation -------------------------------------------------- */

static void derive_hostname(void) {
    char dev[48];
    app_config_get_device_name(dev, sizeof(dev));

    size_t o = 0;
    bool prev_dash = false;
    for (size_t i = 0; dev[i] && o < sizeof(s_hostname) - 1; i++) {
        char c = dev[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            s_hostname[o++] = c;
            prev_dash = false;
        } else if (!prev_dash && o > 0) {
            s_hostname[o++] = '-';
            prev_dash = true;
        }
    }
    while (o > 0 && s_hostname[o - 1] == '-') o--; /* trim trailing dash */
    s_hostname[o] = '\0';
    if (s_hostname[0] == '\0') {
        strlcpy(s_hostname, "allsky-display", sizeof(s_hostname));
    }
}

/* ---- Reason classification ------------------------------------------------ */

static bool reason_is_definite(int reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_ASSOC_FAIL:
            return true;
        default:
            return false;
    }
}

/* ---- Event handlers ------------------------------------------------------- */

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    switch (id) {
        case WIFI_EVENT_STA_START:
            s_started = true;
            break;
        case WIFI_EVENT_STA_CONNECTED:
            s_link_up = true;
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)data;
            s_link_up = false;
            s_got_ip = false;
            s_disc_reason = e ? e->reason : 0;
            s_disc_pending = true;
            break;
        }
        case WIFI_EVENT_SCAN_DONE: {
            if (!s_scan_mutex) break;
            xSemaphoreTake(s_scan_mutex, portMAX_DELAY);
            uint16_t n = 0;
            esp_wifi_scan_get_ap_num(&n);
            if (n > SCAN_MAX_RECORDS) n = SCAN_MAX_RECORDS;
            if (s_scan_cache && n > 0) {
                esp_wifi_scan_get_ap_records(&n, s_scan_cache);
                s_scan_count = n;
            } else {
                esp_wifi_clear_ap_list();
                s_scan_count = 0;
            }
            s_scan_ready = true;
            s_scan_running = false;
            xSemaphoreGive(s_scan_mutex);
            break;
        }
        default:
            break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (id != IP_EVENT_STA_GOT_IP) return;
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&e->ip_info.ip));
    snprintf(s_gw, sizeof(s_gw), IPSTR, IP2STR(&e->ip_info.gw));

    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
        snprintf(s_dns, sizeof(s_dns), IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    }
    s_got_ip = true;
}

/* ---- Init ----------------------------------------------------------------- */

esp_err_t network_wifi_init(void) {
    if (s_inited) return ESP_OK;

    /* Defensive: main.c may also init these; already-inited is not an error. */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    s_event_group = xEventGroupCreate();
    s_scan_mutex = xSemaphoreCreateMutex();
    if (!s_event_group || !s_scan_mutex) return ESP_ERR_NO_MEM;

    s_scan_cache = heap_caps_malloc(sizeof(wifi_ap_record_t) * SCAN_MAX_RECORDS, MALLOC_CAP_SPIRAM);
    if (!s_scan_cache) return ESP_ERR_NO_MEM;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) return ESP_FAIL;

    /* Platform quirk (network.md overview 1): unbound UDP egresses the default
     * netif; force the STA netif to be it so SNTP reaches the server. */
    esp_netif_set_default_netif(s_sta_netif);

    derive_hostname();
    esp_netif_set_hostname(s_sta_netif, s_hostname);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               ip_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(s_mac, sizeof(s_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    app_config_get_wifi_ssid(s_ssid, sizeof(s_ssid));
    app_config_get_wifi_pwd(s_pwd, sizeof(s_pwd));

    s_inited = true;
    ESP_LOGI(TAG, "WiFi init complete, hostname '%s'", s_hostname);
    return ESP_OK;
}

/* Load the station config (ssid/pwd, no BSSID pin) and issue connect. */
static esp_err_t issue_connect(const uint8_t *bssid, uint8_t channel) {
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_pwd, sizeof(wc.sta.password));
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    if (bssid) {
        wc.sta.bssid_set = true;
        memcpy(wc.sta.bssid, bssid, 6);
        wc.sta.channel = channel;
    }
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wc);
    if (err != ESP_OK) return err;
    return esp_wifi_connect();
}

static void begin_attempt(void) {
    s_got_ip = false;
    s_disc_pending = false;
    s_attempt_start_ms = now_ms();
    s_last_attempt_ms = s_attempt_start_ms;
    esp_err_t err = issue_connect(NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(err));
        s_disc_pending = true;
        s_disc_reason = 0;
    }
    s_state = NET_CONNECTING;
    ESP_LOGI(TAG, "Connecting to '%s'", s_ssid);
}

static void enter_connected(bool from_roam) {
    s_state = NET_CONNECTED;
    s_backoff_ms = BACKOFF_MIN_MS;
    xEventGroupSetBits(s_event_group, NETWORK_WIFI_CONNECTED_BIT);

    /* A from_roam arrival is a directed re-association to a different BSSID for
     * the same SSID (evaluate_roam_candidate only targets a distinct BSSID). */
    if (from_roam) {
        system_health_record_roam();
    }

    wifi_ap_record_t ap;
    int rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    ESP_LOGI(TAG, "%s: ip=%s gw=%s dns=%s rssi=%d",
             from_roam ? "Roamed" : "Connected", s_ip, s_gw, s_dns, rssi);

    /* Head-start the clock: SNTP now, first HTTP-Date sync in 8 s. */
    network_time_on_connected();
}

static void fail_attempt(const char *why) {
    ESP_LOGW(TAG, "Connect failed (%s), backoff %u ms", why, (unsigned)s_backoff_ms);
    esp_wifi_disconnect();
    s_backoff_ms *= 2;
    if (s_backoff_ms > BACKOFF_MAX_MS) s_backoff_ms = BACKOFF_MAX_MS;
    s_state = NET_IDLE;
    s_last_attempt_ms = now_ms();
}

/* ---- Roaming -------------------------------------------------------------- */

static void maybe_start_roam_scan(void) {
    int64_t t = now_ms();
    if (t - s_last_roam_scan_ms < ROAM_SCAN_INTERVAL) return;
    if (s_scan_running) return;
    if (network_time_sntp_in_progress()) return;

    /* Skip when the current link is already strong; roaming could not improve it
     * enough to be worth a scan. Fail open if the AP info is unavailable. */
    wifi_ap_record_t cur;
    if (esp_wifi_sta_get_ap_info(&cur) == ESP_OK && cur.rssi > ROAM_SCAN_RSSI_GATE) {
        s_last_roam_scan_ms = t;
        return;
    }

    wifi_scan_config_t sc = { .show_hidden = false };
    s_scan_is_roam = true;
    s_scan_ready = false;
    s_scan_running = true;
    s_last_roam_scan_ms = t;
    s_last_scan_start_ms = t;
    if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
        s_scan_running = false;
    } else {
        system_health_record_roam_scan();
    }
}

static void evaluate_roam_candidate(void) {
    wifi_ap_record_t cur;
    if (esp_wifi_sta_get_ap_info(&cur) != ESP_OK) return;

    int best_rssi = cur.rssi;
    const wifi_ap_record_t *best = NULL;

    xSemaphoreTake(s_scan_mutex, portMAX_DELAY);
    for (int i = 0; i < s_scan_count; i++) {
        const wifi_ap_record_t *r = &s_scan_cache[i];
        if (strncmp((const char *)r->ssid, s_ssid, 32) != 0) continue;
        if (memcmp(r->bssid, cur.bssid, 6) == 0) continue;
        if (r->rssi > best_rssi) {
            best_rssi = r->rssi;
            best = r;
        }
    }

    bool qualifies = best && (best->rssi - cur.rssi) > ROAM_RSSI_MARGIN_DB;
    if (qualifies) {
        memcpy(s_roam_bssid, best->bssid, 6);
        s_roam_channel = best->primary;
    }
    xSemaphoreGive(s_scan_mutex);

    if (!qualifies) return;

    ESP_LOGI(TAG, "Roaming to stronger BSSID (%d -> %d dBm)", cur.rssi, best_rssi);
    xEventGroupClearBits(s_event_group, NETWORK_WIFI_CONNECTED_BIT);
    esp_wifi_disconnect();
    s_roam_start_ms = now_ms();
    s_roam_join_issued = false;
    s_state = NET_ROAMING;
}

static void update_roaming(void) {
    int64_t t = now_ms();
    if (!s_roam_join_issued) {
        /* Wait for the pre-roam disconnect to complete, then directed-join. */
        if (!s_link_up || (t - s_roam_start_ms) > ROAM_DISC_WAIT_MS) {
            s_got_ip = false;
            s_disc_pending = false;
            if (issue_connect(s_roam_bssid, s_roam_channel) != ESP_OK) {
                ESP_LOGW(TAG, "Roam join failed to start");
                fail_attempt("roam-join-start");
                return;
            }
            s_roam_join_issued = true;
            s_roam_start_ms = t;
        }
        return;
    }

    if (s_got_ip) {
        enter_connected(true);
        return;
    }
    if (s_disc_pending || (t - s_roam_start_ms) > ROAM_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Roam failed, falling back to normal reconnect");
        esp_wifi_disconnect();
        s_backoff_ms = BACKOFF_MIN_MS;
        s_state = NET_IDLE;
        s_last_attempt_ms = now_ms();
    }
}

/* ---- Main-loop tick ------------------------------------------------------- */

void network_wifi_update(void) {
    if (!s_inited) return;
    int64_t t = now_ms();

    switch (s_state) {
    case NET_IDLE:
        if (s_ssid[0] == '\0') {
            if (!s_ssid_err_logged) {
                ESP_LOGE(TAG, "WiFi SSID not configured");
                s_ssid_err_logged = true;
            }
            break;
        }
        if ((t - s_last_attempt_ms) >= (int64_t)s_backoff_ms) {
            begin_attempt();
        }
        break;

    case NET_CONNECTING:
        if (s_got_ip) {
            enter_connected(false);
        } else if (s_disc_pending) {
            char msg[48];
            snprintf(msg, sizeof(msg), "reason %d%s", s_disc_reason,
                     reason_is_definite(s_disc_reason) ? " (definite)" : "");
            fail_attempt(msg);
        } else if ((t - s_attempt_start_ms) > CONNECT_TIMEOUT_MS) {
            fail_attempt("timeout");
        }
        break;

    case NET_CONNECTED:
        if (!s_link_up || !s_got_ip) {
            ESP_LOGW(TAG, "Connection lost");
            /* Unplanned connected->disconnected edge. A roam leaves this state via
             * NET_ROAMING instead, so it is not double-counted here. */
            system_health_record_network_disconnect();
            xEventGroupClearBits(s_event_group, NETWORK_WIFI_CONNECTED_BIT);
            s_state = NET_IDLE;
            s_last_attempt_ms = t;
            break;
        }
        if (s_scan_ready && s_scan_is_roam) {
            s_scan_ready = false;
            s_scan_is_roam = false;
            evaluate_roam_candidate();
        } else {
            maybe_start_roam_scan();
        }
        break;

    case NET_ROAMING:
        update_roaming();
        break;
    }

    /* Edge logging for observers. */
    bool now_conn = (s_state == NET_CONNECTED);
    if (now_conn != s_prev_connected) {
        if (now_conn) {
            ESP_LOGI(TAG, "Link up: %s", s_ip);
        } else {
            ESP_LOGI(TAG, "Link down");
        }
        s_prev_connected = now_conn;
    }
}

bool network_wifi_is_connected(void) {
    return s_state == NET_CONNECTED && s_link_up && s_got_ip;
}

esp_err_t network_wifi_connect_start(void) {
    if (!s_inited) return ESP_ERR_INVALID_STATE;
    app_config_get_wifi_ssid(s_ssid, sizeof(s_ssid));
    app_config_get_wifi_pwd(s_pwd, sizeof(s_pwd));
    if (s_ssid[0] == '\0') {
        ESP_LOGE(TAG, "WiFi SSID not configured");
        return ESP_ERR_INVALID_STATE;
    }
    s_last_attempt_ms = -1000000; /* allow immediate first attempt */
    s_state = NET_IDLE;
    return ESP_OK;
}

void network_wifi_get_status(network_wifi_status_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    bool conn = network_wifi_is_connected();
    out->connected = conn;
    strlcpy(out->mac, s_mac, sizeof(out->mac));
    strlcpy(out->hostname, s_hostname, sizeof(out->hostname));

    if (conn) {
        strlcpy(out->ip, s_ip, sizeof(out->ip));
        strlcpy(out->gateway, s_gw, sizeof(out->gateway));
        strlcpy(out->dns, s_dns, sizeof(out->dns));
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            out->rssi = ap.rssi;
            strlcpy(out->ssid, (const char *)ap.ssid, sizeof(out->ssid));
            snprintf(out->bssid, sizeof(out->bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                     ap.bssid[0], ap.bssid[1], ap.bssid[2],
                     ap.bssid[3], ap.bssid[4], ap.bssid[5]);
        }
    } else {
        strlcpy(out->ip, "Not connected", sizeof(out->ip));
    }
}

const char *network_wifi_get_hostname(void) {
    return s_hostname;
}

EventGroupHandle_t network_wifi_event_group(void) {
    return s_event_group;
}

/* ---- Scan JSON for the web UI --------------------------------------------- */

static const char *encryption_str(wifi_auth_mode_t a) {
    switch (a) {
        case WIFI_AUTH_OPEN:          return "Open";
        case WIFI_AUTH_WEP:           return "WEP";
        case WIFI_AUTH_WPA_PSK:       return "WPA";
        case WIFI_AUTH_WPA2_PSK:      return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:  return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
        case WIFI_AUTH_WPA3_PSK:      return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
        default:                      return "Unknown";
    }
}

static const char *quality_str(int rssi) {
    if (rssi > -50) return "Excellent";
    if (rssi > -60) return "Good";
    if (rssi > -70) return "Fair";
    if (rssi > -80) return "Poor";
    return "Weak";
}

/* Append a JSON-escaped string (without surrounding quotes) to buf. */
static int json_escape(char *dst, size_t cap, const char *src) {
    size_t o = 0;
    for (size_t i = 0; src[i] && o + 2 < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            dst[o++] = '\\';
            dst[o++] = (char)c;
        } else if (c < 0x20) {
            if (o + 6 >= cap) break;
            o += snprintf(dst + o, cap - o, "\\u%04x", c);
        } else {
            dst[o++] = (char)c;
        }
    }
    dst[o] = '\0';
    return (int)o;
}

int network_wifi_scan_json(char *buf, size_t buf_len) {
    if (!s_inited || !buf || buf_len < 32) return -1;

    int64_t t = now_ms();
    if (t - s_last_scan_start_ms < SCAN_RATE_LIMIT_MS && !s_scan_running) {
        return -2;
    }

    if (!s_scan_running) {
        wifi_scan_config_t sc = { .show_hidden = false };
        s_scan_is_roam = false;
        s_scan_ready = false;
        s_scan_running = true;
        s_last_scan_start_ms = t;
        if (esp_wifi_scan_start(&sc, false) != ESP_OK) {
            s_scan_running = false;
            return -1;
        }
    }

    /* Wait (bounded) for the async scan to complete. */
    int64_t deadline = t + SCAN_UI_WAIT_MS;
    while (s_scan_running && now_ms() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (s_scan_running) return -1;

    xSemaphoreTake(s_scan_mutex, portMAX_DELAY);
    size_t o = 0;
    o += snprintf(buf + o, buf_len - o, "{\"status\":\"success\",\"networks\":[");
    int count = 0;
    for (int i = 0; i < s_scan_count && o + 160 < buf_len; i++) {
        const wifi_ap_record_t *r = &s_scan_cache[i];
        if (r->primary < 1 || r->primary > 13) continue; /* 2.4 GHz only */

        char esc[100];
        if (r->ssid[0] == '\0') {
            strlcpy(esc, "[Hidden Network]", sizeof(esc));
        } else {
            json_escape(esc, sizeof(esc), (const char *)r->ssid);
        }
        bool is_open = (r->authmode == WIFI_AUTH_OPEN);
        o += snprintf(buf + o, buf_len - o,
                      "%s{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d,"
                      "\"encryption\":\"%s\",\"quality\":\"%s\",\"is_open\":%s}",
                      count ? "," : "", esc, r->rssi, r->primary,
                      encryption_str(r->authmode), quality_str(r->rssi),
                      is_open ? "true" : "false");
        count++;
    }
    xSemaphoreGive(s_scan_mutex);

    o += snprintf(buf + o, buf_len - o, "],\"count\":%d}", count);
    return count;
}
