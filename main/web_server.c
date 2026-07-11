#include "web_server.h"
#include "web_internal.h"
#include "web_route_auth.h"
#include <string.h>
#include <stdio.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "web_server";

/* ---- Server + auth state ------------------------------------------------- */

static httpd_handle_t s_server = NULL;
static bool           s_auth_enabled = false;
static char           s_admin_password[64] = "";

/* ---- Session store (lifted from NINA web_server.c) ----------------------- */

#define MAX_SESSIONS          8
#define SESSION_TOKEN_BYTES   32
#define SESSION_TOKEN_HEX_LEN (SESSION_TOKEN_BYTES * 2)
#define SESSION_TTL_SEC       (12 * 3600)

typedef struct {
    char    token[SESSION_TOKEN_HEX_LEN + 1];
    int64_t expires_us;
} session_t;

static session_t     s_sessions[MAX_SESSIONS];
static portMUX_TYPE  s_sessions_mux = portMUX_INITIALIZER_UNLOCKED;

static void hex_encode(const uint8_t *in, size_t in_len, char *out)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < in_len; i++) {
        out[i * 2]     = hex[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[in[i] & 0xF];
    }
    out[in_len * 2] = '\0';
}

const char *web_session_create(void)
{
    uint8_t raw[SESSION_TOKEN_BYTES];
    esp_fill_random(raw, sizeof(raw));
    char hex[SESSION_TOKEN_HEX_LEN + 1];
    hex_encode(raw, sizeof(raw), hex);

    int64_t expiry = esp_timer_get_time() + (int64_t)SESSION_TTL_SEC * 1000000LL;
    const char *result = NULL;

    taskENTER_CRITICAL(&s_sessions_mux);
    int target = -1;
    int64_t oldest = INT64_MAX;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_sessions[i].token[0] == '\0') { target = i; break; }
        if (s_sessions[i].expires_us < oldest) { oldest = s_sessions[i].expires_us; target = i; }
    }
    memcpy(s_sessions[target].token, hex, sizeof(hex));
    s_sessions[target].expires_us = expiry;
    result = s_sessions[target].token;
    taskEXIT_CRITICAL(&s_sessions_mux);
    return result;
}

static bool session_valid(const char *token)
{
    if (!token || strlen(token) != SESSION_TOKEN_HEX_LEN) return false;
    int64_t now = esp_timer_get_time();
    bool ok = false;
    taskENTER_CRITICAL(&s_sessions_mux);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_sessions[i].token[0] == '\0') continue;
        if (s_sessions[i].expires_us <= now) { memset(&s_sessions[i], 0, sizeof(s_sessions[i])); continue; }
        unsigned char diff = 0;
        for (size_t k = 0; k < SESSION_TOKEN_HEX_LEN; k++)
            diff |= (unsigned char)s_sessions[i].token[k] ^ (unsigned char)token[k];
        if (diff == 0) { ok = true; break; }
    }
    taskEXIT_CRITICAL(&s_sessions_mux);
    return ok;
}

static void session_destroy(const char *token)
{
    if (!token || token[0] == '\0') return;
    taskENTER_CRITICAL(&s_sessions_mux);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (s_sessions[i].token[0] == '\0') continue;
        if (strncmp(s_sessions[i].token, token, SESSION_TOKEN_HEX_LEN) == 0) {
            memset(&s_sessions[i], 0, sizeof(s_sessions[i]));
            break;
        }
    }
    taskEXIT_CRITICAL(&s_sessions_mux);
}

static bool session_extract_cookie(httpd_req_t *req, char *out, size_t out_len)
{
    if (out_len < SESSION_TOKEN_HEX_LEN + 1) return false;
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Cookie");
    if (hdr_len == 0 || hdr_len > 1024) return false;
    char *buf = malloc(hdr_len + 1);
    if (!buf) return false;
    bool ok = false;
    if (httpd_req_get_hdr_value_str(req, "Cookie", buf, hdr_len + 1) == ESP_OK) {
        const char *p = buf;
        while (p && *p) {
            while (*p == ' ' || *p == ';') p++;
            if (strncmp(p, "session=", 8) == 0) {
                p += 8;
                size_t n = 0;
                while (p[n] && p[n] != ';' && n < out_len - 1) { out[n] = p[n]; n++; }
                out[n] = '\0';
                ok = (n == SESSION_TOKEN_HEX_LEN);
                break;
            }
            const char *next = strchr(p, ';');
            if (!next) break;
            p = next + 1;
        }
    }
    free(buf);
    return ok;
}

/* ---- Auth surface (declared in web_internal.h) --------------------------- */

bool web_auth_enabled(void) { return s_auth_enabled; }

void web_auth_set(bool enabled, const char *password)
{
    s_auth_enabled = enabled;
    if (password) {
        strncpy(s_admin_password, password, sizeof(s_admin_password) - 1);
        s_admin_password[sizeof(s_admin_password) - 1] = '\0';
    }
}

void web_auth_configure(bool enabled, const char *password) { web_auth_set(enabled, password); }
bool web_auth_is_enabled(void) { return s_auth_enabled; }

bool web_is_setup_mode(void)
{
    /* The main server only runs with a station connection; provisioning uses the
     * captive portal. Setup mode here means no stored WiFi provisioning. */
    return !app_config_get_wifi_prov();
}

bool web_check_session(httpd_req_t *req)
{
    if (!s_auth_enabled) return true;

    char tok[SESSION_TOKEN_HEX_LEN + 1];
    if (session_extract_cookie(req, tok, sizeof(tok)) && session_valid(tok)) return true;

    /* Stateless header path for automation clients. */
    size_t hdr_len = httpd_req_get_hdr_value_len(req, "X-Auth-Password");
    if (hdr_len == 0 || hdr_len > sizeof(s_admin_password) - 1) return false;
    char hdr[sizeof(s_admin_password)];
    if (httpd_req_get_hdr_value_str(req, "X-Auth-Password", hdr, sizeof(hdr)) != ESP_OK) return false;

    size_t a = strlen(hdr), b = strlen(s_admin_password);
    unsigned char diff = (a != b) ? 1 : 0;
    size_t maxn = (a > b) ? a : b;
    for (size_t i = 0; i < maxn; i++) {
        unsigned char ca = (i < a) ? (unsigned char)hdr[i] : 0;
        unsigned char cb = (i < b) ? (unsigned char)s_admin_password[i] : 0;
        diff |= ca ^ cb;
    }
    return (diff == 0 && s_admin_password[0] != '\0');
}

bool web_auth_password_matches(const char *pw)
{
    if (!pw || s_admin_password[0] == '\0') return false;
    size_t a = strlen(pw), b = strlen(s_admin_password);
    unsigned char diff = (a != b) ? 1 : 0;
    size_t maxn = (a > b) ? a : b;
    for (size_t i = 0; i < maxn; i++) {
        unsigned char ca = (i < a) ? (unsigned char)pw[i] : 0;
        unsigned char cb = (i < b) ? (unsigned char)s_admin_password[i] : 0;
        diff |= ca ^ cb;
    }
    return diff == 0;
}

void web_session_destroy_from_req(httpd_req_t *req)
{
    char tok[SESSION_TOKEN_HEX_LEN + 1];
    if (session_extract_cookie(req, tok, sizeof(tok))) session_destroy(tok);
}

esp_err_t web_send_auth_required(httpd_req_t *req)
{
    if (strncmp(req->uri, "/api/", 5) != 0) {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "/login");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"error\",\"message\":\"authentication required\"}",
                           HTTPD_RESP_USE_STRLEN);
}

/* ---- Firmware / system info helpers -------------------------------------- */

void web_fw_checksum8(char *out8)
{
    const esp_app_desc_t *d = esp_app_get_description();
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 4; i++) {
        out8[i * 2]     = hex[(d->app_elf_sha256[i] >> 4) & 0xF];
        out8[i * 2 + 1] = hex[d->app_elf_sha256[i] & 0xF];
    }
    out8[8] = '\0';
}

void web_fw_checksum_full(char *out, size_t len)
{
    const esp_app_desc_t *d = esp_app_get_description();
    static const char hex[] = "0123456789abcdef";
    size_t o = 0;
    for (int i = 0; i < 32 && o + 2 < len; i++) {
        out[o++] = hex[(d->app_elf_sha256[i] >> 4) & 0xF];
        out[o++] = hex[d->app_elf_sha256[i] & 0xF];
    }
    if (o < len) out[o] = '\0';
}

uint32_t web_fw_app_size(void)
{
    /* Running partition size is the available upper bound on the app image. */
    const esp_partition_t *run = esp_ota_get_running_partition();
    return run ? run->size : 0;
}

uint32_t web_fw_free_space(void)
{
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    return next ? next->size : 0;
}

const char *web_fw_version(void)
{
    return esp_app_get_description()->version;
}

/* ---- Route table + auth gate (lifted from NINA) -------------------------- */

typedef struct {
    httpd_uri_t        uri;
    route_auth_class_t auth;
} route_entry_t;

static esp_err_t auth_gate_handler(httpd_req_t *req)
{
    const route_entry_t *entry = (const route_entry_t *)req->user_ctx;
    bool setup_mode = web_is_setup_mode();

    switch (entry->auth) {
        case ROUTE_PUBLIC:
            return entry->uri.handler(req);
        case ROUTE_SETUP_EXEMPT:
            if (setup_mode) return entry->uri.handler(req);
            break;
        default:
            break;
    }
    bool authed = web_check_session(req);
    if (!route_auth_allows(entry->auth, setup_mode, authed)) return web_send_auth_required(req);
    return entry->uri.handler(req);
}

static esp_err_t err_404_trampoline(httpd_req_t *req, httpd_err_code_t err)
{
    return web_page_404_handler(req, err);
}

/*
 * Route table. Every SPA page path serves the same shell so a deep-link refresh
 * works. Fail-closed: any entry omitting .auth defaults to ROUTE_AUTH_REQUIRED.
 * PUBLIC/SETUP_EXEMPT allowlist per web-server.md OQ9.
 */
static const route_entry_t s_routes[] = {
    /* -- Pages (SPA shell) -- */
    { { "/",                HTTP_GET, web_page_app_handler, NULL }, ROUTE_SETUP_EXEMPT },
    { { "/console",         HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/network",  HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/mqtt",     HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/images",   HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/display",  HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/system",   HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/config/commands", HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api-reference",   HTTP_GET, web_page_app_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/favicon.ico",     HTTP_GET, web_page_favicon_handler, NULL }, ROUTE_PUBLIC },
    { { "/login",           HTTP_GET, web_page_login_handler, NULL }, ROUTE_PUBLIC },
    { { "/api/login",       HTTP_POST, web_login_post_handler, NULL }, ROUTE_PUBLIC },
    { { "/api/logout",      HTTP_POST, web_logout_post_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/auth/status", HTTP_GET, web_auth_status_handler, NULL }, ROUTE_PUBLIC },

    /* -- WebSocket log console -- */
    { { "/ws/logs",         HTTP_GET, web_ws_handler, NULL }, ROUTE_AUTH_REQUIRED },

    /* -- Read-only JSON -- */
    { { "/status",          HTTP_GET, web_status_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/info",        HTTP_GET, web_info_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/health",      HTTP_GET, web_health_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/wifi-scan",   HTTP_GET, web_wifi_scan_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/screenshot",  HTTP_GET, web_screenshot_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/current-image", HTTP_GET, web_current_image_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/images/state", HTTP_GET, web_images_state_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/getMoon",     HTTP_GET, web_get_moon_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/backup",      HTTP_GET, web_backup_handler, NULL }, ROUTE_AUTH_REQUIRED },

    /* -- Config / system mutating -- */
    { { "/api/save",        HTTP_POST, web_save_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/restore",     HTTP_POST, web_restore_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/restart",     HTTP_POST, web_restart_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/factory-reset", HTTP_POST, web_factory_reset_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/set-log-severity", HTTP_POST, web_set_log_severity_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/clear-crash-logs", HTTP_POST, web_clear_crash_logs_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/force-brightness-update", HTTP_POST, web_force_brightness_handler, NULL }, ROUTE_AUTH_REQUIRED },

    /* -- Image source management -- */
    { { "/api/add-source",  HTTP_POST, web_add_source_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/addPreset",   HTTP_POST, web_add_preset_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/remove-source", HTTP_POST, web_remove_source_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/update-source", HTTP_POST, web_update_source_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/clear-sources", HTTP_POST, web_clear_sources_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/bulk-delete-sources", HTTP_POST, web_bulk_delete_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/next-image",  HTTP_POST, web_next_image_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/force-refresh", HTTP_POST, web_force_refresh_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/update-transform", HTTP_POST, web_update_transform_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/copy-defaults", HTTP_POST, web_copy_defaults_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/apply-transform", HTTP_POST, web_apply_transform_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/toggle-image-enabled", HTTP_POST, web_toggle_enabled_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/select-image", HTTP_POST, web_select_image_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/clear-editing-state", HTTP_POST, web_clear_editing_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/images/tune", HTTP_POST, web_tune_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/images/tune/stop", HTTP_POST, web_tune_stop_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/update-image-duration", HTTP_POST, web_update_duration_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/setMoon",     HTTP_POST, web_set_moon_handler, NULL }, ROUTE_AUTH_REQUIRED },

    /* -- OTA -- */
    { { "/update",          HTTP_GET,  web_ota_page_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/update",          HTTP_POST, web_ota_upload_handler, NULL }, ROUTE_AUTH_REQUIRED },
    { { "/api/ota-github",  HTTP_POST, web_ota_github_handler, NULL }, ROUTE_AUTH_REQUIRED },
};

#define WEB_ROUTE_COUNT (sizeof(s_routes) / sizeof(s_routes[0]))
#define WEB_MAX_URI_HANDLERS 60
_Static_assert(WEB_ROUTE_COUNT <= WEB_MAX_URI_HANDLERS, "max_uri_handlers too small");

esp_err_t web_server_start(void)
{
    if (s_server) return ESP_OK;

    web_screenshot_encoder_init();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port     = WEB_PORT;
    config.stack_size      = 16384;
    config.max_uri_handlers = WEB_MAX_URI_HANDLERS;
    config.max_open_sockets = 10;
    config.lru_purge_enable = true;
    config.keep_alive_enable = true;
    config.keep_alive_idle   = 5;
    config.keep_alive_interval = 3;
    config.keep_alive_count  = 3;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    for (size_t i = 0; i < WEB_ROUTE_COUNT; i++) {
        httpd_uri_t gated = s_routes[i].uri;
        gated.handler  = auth_gate_handler;
        gated.user_ctx = (void *)&s_routes[i];
        if (strcmp(s_routes[i].uri.uri, "/ws/logs") == 0) {
            gated.is_websocket = true;
            gated.handle_ws_control_frames = false;
        }
        esp_err_t r = httpd_register_uri_handler(s_server, &gated);
        if (r != ESP_OK) ESP_LOGE(TAG, "register %s failed: %s", s_routes[i].uri.uri, esp_err_to_name(r));
    }

    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, err_404_trampoline);

    web_ws_init(s_server);

    ESP_LOGI(TAG, "Web server up on :%d (%d routes, auth %s)",
             WEB_PORT, (int)WEB_ROUTE_COUNT, s_auth_enabled ? "on" : "off");
    return ESP_OK;
}

void web_server_stop(void)
{
    if (!s_server) return;
    web_ws_deinit();
    httpd_stop(s_server);
    s_server = NULL;
    ESP_LOGI(TAG, "Web server stopped");
}

bool web_server_is_running(void) { return s_server != NULL; }
