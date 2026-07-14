#pragma once

/*
 * Shared declarations for the AllSky web server handler modules. Not a public
 * header: only the web_*.c translation units include it.
 */

#include <stdint.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "app_config.h"
#include "web_util.h"
#include "web_integration.h"

/* Ports and identity. */
#define WEB_PORT            8080
#define WEB_MAX_SOURCES     ALLSKY_MAX_SOURCES

/* MAX_SCALE clamp exposed to the UI (config clamps scale to [0.1, 2.0]). */
#define WEB_MAX_SCALE       2.0f

/* ---- Auth / session backend (web_server.c) ------------------------------- */
bool        web_auth_enabled(void);
void        web_auth_set(bool enabled, const char *password);
bool        web_check_session(httpd_req_t *req);       /* true = authorized */
const char *web_session_create(void);
void        web_session_destroy_from_req(httpd_req_t *req);
bool        web_is_setup_mode(void);
esp_err_t   web_send_auth_required(httpd_req_t *req);

/* ---- WebSocket log console (web_ws.c) ------------------------------------ */
esp_err_t   web_ws_init(httpd_handle_t server);
void        web_ws_deinit(void);
esp_err_t   web_ws_handler(httpd_req_t *req);
void        web_ws_set_min_severity(int sev);          /* 0..4 */
void        web_ws_set_ota_suppress(bool suppress);

/* ---- Screenshot (web_screenshot.c) --------------------------------------- */
void        web_screenshot_encoder_init(void);
esp_err_t   web_screenshot_handler(httpd_req_t *req);

/* ---- OTA (web_ota.c) ----------------------------------------------------- */
void        web_ota_mark_valid_after_boot(void);       /* call once early at boot */
esp_err_t   web_ota_page_handler(httpd_req_t *req);     /* GET /update UI */
esp_err_t   web_ota_upload_handler(httpd_req_t *req);   /* POST /update multipart */
esp_err_t   web_ota_github_handler(httpd_req_t *req);   /* POST /api/ota-github */

/* ---- Page handlers (web_pages.c) ----------------------------------------- */
esp_err_t   web_page_app_handler(httpd_req_t *req);     /* SPA shell for all pages */
esp_err_t   web_page_login_handler(httpd_req_t *req);
esp_err_t   web_page_favicon_handler(httpd_req_t *req);
esp_err_t   web_page_404_handler(httpd_req_t *req, httpd_err_code_t err);
esp_err_t   web_login_post_handler(httpd_req_t *req);
esp_err_t   web_logout_post_handler(httpd_req_t *req);
esp_err_t   web_auth_status_handler(httpd_req_t *req);

/* ---- Read-only JSON (web_api_read.c) ------------------------------------- */
esp_err_t   web_status_handler(httpd_req_t *req);
esp_err_t   web_info_handler(httpd_req_t *req);
esp_err_t   web_health_handler(httpd_req_t *req);
esp_err_t   web_crash_log_handler(httpd_req_t *req);
esp_err_t   web_wifi_scan_handler(httpd_req_t *req);
esp_err_t   web_current_image_handler(httpd_req_t *req);
esp_err_t   web_images_state_handler(httpd_req_t *req);
esp_err_t   web_get_moon_handler(httpd_req_t *req);

/* ---- Config / system mutating (web_api_config.c) ------------------------- */
esp_err_t   web_save_handler(httpd_req_t *req);
esp_err_t   web_backup_handler(httpd_req_t *req);
esp_err_t   web_restore_handler(httpd_req_t *req);
esp_err_t   web_restart_handler(httpd_req_t *req);
esp_err_t   web_factory_reset_handler(httpd_req_t *req);
esp_err_t   web_set_log_severity_handler(httpd_req_t *req);
esp_err_t   web_clear_crash_logs_handler(httpd_req_t *req);
esp_err_t   web_force_brightness_handler(httpd_req_t *req);

/* ---- Image source management (web_api_images.c) -------------------------- */
esp_err_t   web_add_source_handler(httpd_req_t *req);
esp_err_t   web_add_preset_handler(httpd_req_t *req);
esp_err_t   web_remove_source_handler(httpd_req_t *req);
esp_err_t   web_update_source_handler(httpd_req_t *req);
esp_err_t   web_clear_sources_handler(httpd_req_t *req);
esp_err_t   web_bulk_delete_handler(httpd_req_t *req);
esp_err_t   web_next_image_handler(httpd_req_t *req);
esp_err_t   web_force_refresh_handler(httpd_req_t *req);
esp_err_t   web_update_transform_handler(httpd_req_t *req);
esp_err_t   web_copy_defaults_handler(httpd_req_t *req);
esp_err_t   web_apply_transform_handler(httpd_req_t *req);
esp_err_t   web_toggle_enabled_handler(httpd_req_t *req);
esp_err_t   web_select_image_handler(httpd_req_t *req);
esp_err_t   web_clear_editing_handler(httpd_req_t *req);
esp_err_t   web_tune_handler(httpd_req_t *req);
esp_err_t   web_tune_stop_handler(httpd_req_t *req);
esp_err_t   web_update_duration_handler(httpd_req_t *req);
esp_err_t   web_set_moon_handler(httpd_req_t *req);

/* Preset image catalog (web-server.md 5.2), shared by web_api_read/web_api_images. */
typedef struct {
    const char *id;
    const char *label;
    const char *url;
    int         nominal_px;
    int         crop_pct;
} web_preset_t;
extern const web_preset_t g_web_presets[];
extern const int g_web_preset_count;

/* Editing-pause state shared with the image pipeline via hooks (web_api_images.c). */
void        web_editing_touch(void);   /* mark edit activity (resets backstop) */
bool        web_editing_paused(void);  /* true while paused-for-editing */
int         web_editing_tune_index(void); /* live tune index, -1 when not tuning */
void        web_editing_tick(void);    /* main-loop backstop: auto-resume after 10 min idle */

/* ---- Firmware / system info (web_server.c) ------------------------------- */
void        web_fw_checksum8(char *out8);              /* 8 hex chars + NUL */
void        web_fw_checksum_full(char *out, size_t len);
uint32_t    web_fw_app_size(void);
uint32_t    web_fw_free_space(void);
const char *web_fw_version(void);
