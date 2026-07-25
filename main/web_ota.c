#include "web_internal.h"
#include "web_server.h"
#include "network_http.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"

static const char *TAG = "web_ota";

static volatile bool s_ota_active = false;

static void ota_wdt_feed(void)
{
    if (esp_task_wdt_status(NULL) == ESP_OK) esp_task_wdt_reset();
}

bool web_ota_in_progress(void)
{
    return s_ota_active;
}

void web_ota_mark_valid_after_boot(void)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (run && esp_ota_get_state_partition(run, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "OTA image confirmed valid; rollback cancelled");
        }
    }
}

/* ---- GET /update : minimal upload UI ------------------------------------- */

static const char OTA_PAGE[] =
"<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Firmware Update</title><style>body{font-family:system-ui;background:#0a0c14;color:#e2e8f0;padding:24px}"
".c{max-width:460px;margin:0 auto;background:#14171f;border:1px solid #ffffff14;border-radius:10px;padding:24px}"
"h1{font-size:18px}input,button{font-size:14px;margin-top:12px}button{background:#5a86e8;color:#fff;border:0;"
"padding:10px 16px;border-radius:6px;cursor:pointer}#p{margin-top:14px;height:8px;background:#0c0e14;border-radius:4px;overflow:hidden}"
"#b{height:100%;width:0;background:#4ade80;transition:width .2s}#s{margin-top:10px;font-size:13px;color:#94a3b8}</style></head>"
"<body><div class=c><h1>Firmware Update</h1>"
"<input type=file id=f accept='.bin'><br><button onclick='up()'>Upload &amp; Flash</button>"
"<div id=p><div id=b></div></div><div id=s>Select a .bin firmware image.</div>"
"<p style='margin-top:16px'><a href='/config/system' style='color:#7aa2f7'>Back to System</a></p></div>"
"<script>function up(){var f=document.getElementById('f').files[0];if(!f){document.getElementById('s').textContent='No file selected';return;}"
"var fd=new FormData();fd.append('firmware',f);var x=new XMLHttpRequest();x.open('POST','/update');"
"x.upload.onprogress=function(e){if(e.lengthComputable){var pc=Math.round(e.loaded/e.total*100);document.getElementById('b').style.width=pc+'%';document.getElementById('s').textContent='Uploading '+pc+'%';}};"
"x.onload=function(){document.getElementById('s').textContent=x.responseText;if(x.status==200){document.getElementById('b').style.width='100%';setTimeout(function(){location.href='/';},8000);}};"
"x.onerror=function(){document.getElementById('s').textContent='Upload failed';};x.send(fd);}</script></body></html>";

esp_err_t web_ota_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, OTA_PAGE, HTTPD_RESP_USE_STRLEN);
}

/* ---- POST /update : multipart firmware upload ---------------------------- */

static void ota_begin_ui(void)
{
    s_ota_active = true;
    web_ws_set_ota_suppress(true);
    web_hook_ota_screen(0, "Starting...");
    ota_wdt_feed();
}

static void ota_end_ui(bool ok)
{
    web_hook_ota_screen(ok ? 100 : -1, ok ? "Rebooting..." : "OTA Failed");
    web_ws_set_ota_suppress(false);
    s_ota_active = false;
}

esp_err_t web_ota_upload_handler(httpd_req_t *req)
{
    if (s_ota_active) return web_send_status(req, 409, "error", "OTA already in progress", NULL);

    /* Resolve multipart boundary. */
    char ct[160];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) != ESP_OK)
        return web_send_400(req, "Missing Content-Type");
    char *bp = strstr(ct, "boundary=");
    if (!bp) return web_send_400(req, "Missing multipart boundary");
    bp += 9;
    char boundary[96];
    snprintf(boundary, sizeof(boundary), "\r\n--%s", bp);
    size_t blen = strlen(boundary);

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return web_send_status(req, 500, "error", "No OTA partition", NULL);

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(part, OTA_SIZE_UNKNOWN, &handle) != ESP_OK)
        return web_send_status(req, 500, "error", "esp_ota_begin failed", NULL);

    ota_begin_ui();

    /* All working buffers are allocated ONCE here, never per-chunk. Internal
     * heap is tight during normal operation (min ~31 KB); a malloc/free on every
     * 2 KB recv would fragment it and can panic the OTA task. `combined` holds the
     * withheld tail (<= blen) plus one recv (<= 2048). */
    const size_t recv_cap = 2048;
    char *buf = heap_caps_malloc(recv_cap, MALLOC_CAP_SPIRAM);
    char *tail = heap_caps_malloc(blen + 8, MALLOC_CAP_SPIRAM);
    char *combined = heap_caps_malloc(blen + recv_cap + 1, MALLOC_CAP_SPIRAM);
    size_t tail_len = 0;
    bool header_done = false;
    size_t written = 0, total = req->content_len, recv_total = 0;
    int last_pct = -1;
    esp_err_t status = ESP_OK;
    char hdracc[512]; size_t hdracc_len = 0;

    if (!buf || !tail || !combined) { status = ESP_ERR_NO_MEM; goto done; }

    while (recv_total < total) {
        int r = httpd_req_recv(req, buf, 2048);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { status = ESP_FAIL; break; }
        recv_total += r;
        ota_wdt_feed();

        char *data = buf; size_t dlen = r;

        if (!header_done) {
            /* Accumulate until the blank line that ends the part headers.
             * prior = bytes already in hdracc BEFORE this chunk. The copy may be
             * truncated by the 512-byte cap, so it is NOT hdracc_len - dlen. */
            size_t prior = hdracc_len;
            size_t cp = dlen; if (cp > sizeof(hdracc) - hdracc_len - 1) cp = sizeof(hdracc) - hdracc_len - 1;
            memcpy(hdracc + hdracc_len, data, cp);
            hdracc_len += cp;
            hdracc[hdracc_len] = '\0';
            char *sep = strstr(hdracc, "\r\n\r\n");
            if (!sep) {
                /* No blank line yet and the accumulator is full: part headers
                 * larger than our cap, which should never happen for a valid
                 * multipart upload. Fail loud rather than consume the whole body. */
                if (hdracc_len >= sizeof(hdracc) - 1) { status = ESP_FAIL; break; }
                continue;
            }
            size_t consumed_in_hdr = (sep + 4) - hdracc;
            /* Header may span prior chunks; body starts consumed_in_hdr bytes
             * from the start of accumulation. Map into this chunk's offset. */
            size_t body_off = (consumed_in_hdr > prior) ? (consumed_in_hdr - prior) : 0;
            if (body_off > dlen) body_off = dlen;
            data += body_off; dlen -= body_off;
            header_done = true;
        }

        /* Combine withheld tail + new data into the pre-allocated buffer, then
         * withhold a new tail that could contain the closing boundary. */
        size_t combined_len = tail_len + dlen;
        memcpy(combined, tail, tail_len);
        memcpy(combined + tail_len, data, dlen);

        /* Search for closing boundary. */
        char *found = NULL;
        for (size_t i = 0; i + blen <= combined_len; i++) {
            if (memcmp(combined + i, boundary, blen) == 0) { found = combined + i; break; }
        }
        if (found) {
            size_t body = found - combined;
            if (body) { if (esp_ota_write(handle, combined, body) != ESP_OK) status = ESP_FAIL; else written += body; }
            break;  /* reached end of firmware part */
        }

        size_t keep = (combined_len > blen) ? blen : combined_len;
        size_t flush = combined_len - keep;
        if (flush) { if (esp_ota_write(handle, combined, flush) != ESP_OK) status = ESP_FAIL; else written += flush; }
        memcpy(tail, combined + flush, keep);
        tail_len = keep;

        if (total) {
            int pct = (int)(recv_total * 100 / total);
            if (pct / 10 != last_pct / 10) { ESP_LOGI(TAG, "OTA %d%%", pct); web_hook_ota_screen(pct, "Flashing..."); last_pct = pct; }
        }
        ota_wdt_feed();
    }

done:
    free(buf);
    free(tail);
    free(combined);

    if (status != ESP_OK || written == 0) {
        esp_ota_abort(handle);
        ota_end_ui(false);
        return web_send_status(req, 500, "error", "OTA write failed", NULL);
    }
    if (esp_ota_end(handle) != ESP_OK) {
        ota_end_ui(false);
        return web_send_status(req, 500, "error", "OTA image invalid", NULL);
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
        ota_end_ui(false);
        return web_send_status(req, 500, "error", "Set boot partition failed", NULL);
    }

    ota_end_ui(true);
    ESP_LOGI(TAG, "OTA complete: %u bytes; rebooting", (unsigned)written);
    esp_err_t rr = web_send_ok(req, "Update complete. Device rebooting...");
    web_hook_crash_save();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return rr;
}

/* ---- POST /api/ota-github : pull a release binary by URL ------------------ */

esp_err_t web_ota_github_handler(httpd_req_t *req)
{
    if (s_ota_active) return web_send_status(req, 409, "error", "OTA already in progress", NULL);

    char body[2200];
    int n = web_read_body(req, body, sizeof(body));
    if (n < 0) return web_send_400(req, "Invalid request body");

    char url[2048] = "";
    cJSON *j = cJSON_Parse(body);
    if (j) {
        cJSON *u = cJSON_GetObjectItem(j, "url");
        if (cJSON_IsString(u)) strncpy(url, u->valuestring, sizeof(url) - 1);
        cJSON_Delete(j);
    }
    if (url[0] == '\0' && !web_form_get(body, "url", url, sizeof(url)))
        return web_send_400(req, "Missing required parameter: url");
    if (url[0] == '\0') return web_send_400(req, "Missing required parameter: url");

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (!part) return web_send_status(req, 500, "error", "No OTA partition", NULL);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 15000,
        .crt_bundle_attach = NULL,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return web_send_status(req, 500, "error", "HTTP client init failed", NULL);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) { esp_http_client_cleanup(client); return web_send_status(req, 502, "error", "Download connect failed", NULL); }
    int64_t clen = esp_http_client_fetch_headers(client);
    int http_status = esp_http_client_get_status_code(client);
    if (http_status != 200) {
        esp_http_client_close(client); esp_http_client_cleanup(client);
        return web_send_status(req, 502, "error", "Release download HTTP error", NULL);
    }

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(part, OTA_SIZE_UNKNOWN, &handle) != ESP_OK) {
        esp_http_client_close(client); esp_http_client_cleanup(client);
        return web_send_status(req, 500, "error", "esp_ota_begin failed", NULL);
    }

    ota_begin_ui();
    char *buf = malloc(4096);
    size_t written = 0; int last_pct = -1;
    esp_err_t status = ESP_OK;
    if (buf) {
        while (1) {
            int r = esp_http_client_read(client, buf, 4096);
            if (r < 0) { status = ESP_FAIL; break; }
            if (r == 0) break;
            if (esp_ota_write(handle, buf, r) != ESP_OK) { status = ESP_FAIL; break; }
            written += r;
            ota_wdt_feed();
            if (clen > 0) {
                int pct = (int)((int64_t)written * 100 / clen);
                if (pct / 10 != last_pct / 10) { ESP_LOGI(TAG, "GH OTA %d%%", pct); web_hook_ota_screen(pct, "Flashing..."); last_pct = pct; }
            }
        }
        free(buf);
    } else status = ESP_ERR_NO_MEM;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status != ESP_OK || written == 0 || esp_ota_end(handle) != ESP_OK) {
        if (status != ESP_OK || written == 0) esp_ota_abort(handle);
        ota_end_ui(false);
        return web_send_status(req, 500, "error", "OTA flash failed", NULL);
    }
    if (esp_ota_set_boot_partition(part) != ESP_OK) {
        ota_end_ui(false);
        return web_send_status(req, 500, "error", "Set boot partition failed", NULL);
    }
    ota_end_ui(true);
    esp_err_t rr = web_send_ok(req, "GitHub update installed. Rebooting...");
    web_hook_crash_save();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return rr;
}
