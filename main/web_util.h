#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Read the whole request body into a caller buffer (NUL-terminated). Returns the
 * body length, or -1 on error / oversize. */
int web_read_body(httpd_req_t *req, char *buf, size_t buf_len);

/* Extract and URL-decode a field from an x-www-form-urlencoded body. Returns
 * true if present. out is always NUL-terminated. */
bool web_form_get(const char *body, const char *key, char *out, size_t out_len);

/* True if the field key is present in the body at all (even empty). */
bool web_form_has(const char *body, const char *key);

/* Extract and URL-decode a query-string parameter from the request URL. */
bool web_query_get(httpd_req_t *req, const char *key, char *out, size_t out_len);

/* Send the standard {"status":..,"message":..} envelope with extra raw JSON
 * fields appended (extra may be NULL). status_str is "success"/"error"/"queued". */
esp_err_t web_send_status(httpd_req_t *req, int http_code, const char *status_str,
                          const char *message, const char *extra_json);

/* Shorthands. */
esp_err_t web_send_json(httpd_req_t *req, int http_code, const char *json);
esp_err_t web_send_400(httpd_req_t *req, const char *message);
esp_err_t web_send_ok(httpd_req_t *req, const char *message);

/* Append an HTML-escaped copy of src to dst (bounded). Escapes & < > " '. */
void web_html_escape(char *dst, size_t dst_len, const char *src);

/* Append a JSON-escaped copy of src to dst (bounded, no surrounding quotes). */
void web_json_escape(char *dst, size_t dst_len, const char *src);

#ifdef __cplusplus
}
#endif
