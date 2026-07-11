#include "web_internal.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "web_pages";

extern const uint8_t app_html_start[]   asm("_binary_app_html_start");
extern const uint8_t app_html_end[]     asm("_binary_app_html_end");
extern const uint8_t login_html_start[] asm("_binary_login_html_start");
extern const uint8_t login_html_end[]   asm("_binary_login_html_end");

/* Single SPA shell serves every navigable page; client routes on the path. */
esp_err_t web_page_app_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)app_html_start,
                           app_html_end - app_html_start);
}

esp_err_t web_page_login_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)login_html_start,
                           login_html_end - login_html_start);
}

esp_err_t web_page_favicon_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t web_page_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/html");
    static const char page[] =
        "<!doctype html><html><head><meta charset=utf-8><title>Page Not Found</title>"
        "<style>body{font-family:system-ui;background:#0a0c14;color:#e2e8f0;display:flex;"
        "align-items:center;justify-content:center;height:100vh;margin:0}"
        ".c{text-align:center}a{color:#7aa2f7}</style></head><body><div class=c>"
        "<h1>Page Not Found</h1><p>The requested page does not exist.</p>"
        "<p><a href='/'>Return to Dashboard</a></p></div></body></html>";
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

/* ---- Auth endpoints ------------------------------------------------------ */

esp_err_t web_login_post_handler(httpd_req_t *req)
{
    char body[256];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");

    char password[96] = "";
    cJSON *j = cJSON_Parse(body);
    if (j) {
        cJSON *p = cJSON_GetObjectItem(j, "password");
        if (cJSON_IsString(p)) strncpy(password, p->valuestring, sizeof(password) - 1);
        cJSON_Delete(j);
    } else {
        web_form_get(body, "password", password, sizeof(password));
    }

    if (!web_auth_enabled()) {
        /* Auth off: any login succeeds and simply issues a session. */
        const char *tok = web_session_create();
        char cookie[128];
        snprintf(cookie, sizeof(cookie), "session=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=43200", tok);
        httpd_resp_set_hdr(req, "Set-Cookie", cookie);
        return web_send_ok(req, "Logged in");
    }

    /* Verify against the configured admin password via check_session's compare:
     * reuse web_auth by attempting a header-style validation is not exposed here,
     * so compare through a fresh session grant only on match. */
    extern bool web_auth_password_matches(const char *pw);
    if (!web_auth_password_matches(password))
        return web_send_status(req, 401, "error", "Incorrect password", NULL);

    const char *tok = web_session_create();
    char cookie[128];
    snprintf(cookie, sizeof(cookie), "session=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=43200", tok);
    httpd_resp_set_hdr(req, "Set-Cookie", cookie);
    return web_send_ok(req, "Logged in");
}

esp_err_t web_logout_post_handler(httpd_req_t *req)
{
    web_session_destroy_from_req(req);
    httpd_resp_set_hdr(req, "Set-Cookie", "session=; Path=/; HttpOnly; Max-Age=0");
    return web_send_ok(req, "Logged out");
}

esp_err_t web_auth_status_handler(httpd_req_t *req)
{
    char body[96];
    snprintf(body, sizeof(body), "{\"auth_enabled\":%s,\"authed\":%s}",
             web_auth_enabled() ? "true" : "false",
             web_check_session(req) ? "true" : "false");
    (void)TAG;
    return web_send_json(req, 200, body);
}
