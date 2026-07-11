#include "web_internal.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "web_ws";

#define WS_MAX_CLIENTS   4
#define WS_LINE_MAX      384
#define WS_DRAIN_MAX     2048
#define WS_REPLAY_CHUNK  1024
#define WS_REPLAY_MAX    6144

static httpd_handle_t s_server = NULL;
static int            s_clients[WS_MAX_CLIENTS];
static int            s_client_count = 0;
static SemaphoreHandle_t s_lock = NULL;
static TaskHandle_t   s_task = NULL;
static volatile bool  s_run = false;
static volatile bool  s_ota_suppress = false;
static volatile int   s_min_sev = 0;

/* ---- client registry ----------------------------------------------------- */

static void clients_add(int fd)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < s_client_count; i++) if (s_clients[i] == fd) { xSemaphoreGive(s_lock); return; }
    if (s_client_count < WS_MAX_CLIENTS) s_clients[s_client_count++] = fd;
    xSemaphoreGive(s_lock);
}

static void clients_remove(int fd)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < s_client_count; i++) {
        if (s_clients[i] == fd) {
            s_clients[i] = s_clients[--s_client_count];
            break;
        }
    }
    xSemaphoreGive(s_lock);
}

static int clients_snapshot(int *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_client_count;
    memcpy(out, s_clients, n * sizeof(int));
    xSemaphoreGive(s_lock);
    return n;
}

/* ---- sending ------------------------------------------------------------- */

static esp_err_t ws_send_text(int fd, const char *text, size_t len)
{
    httpd_ws_frame_t frame = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)text,
        .len = len,
    };
    return httpd_ws_send_frame_async(s_server, fd, &frame);
}

static void broadcast(const char *text, size_t len)
{
    int fds[WS_MAX_CLIENTS];
    int n = clients_snapshot(fds);
    for (int i = 0; i < n; i++) {
        if (ws_send_text(fds[i], text, len) != ESP_OK) clients_remove(fds[i]);
    }
}

/* Map an ESP-IDF log line's leading level letter to a severity index. */
static int line_severity(const char *line)
{
    /* Skip any leading color-escape sequence. */
    if (line[0] == '\033') { const char *m = strchr(line, 'm'); if (m) line = m + 1; }
    switch (line[0]) {
        case 'V': case 'D': return 0;   /* DEBUG */
        case 'I': return 1;             /* INFO */
        case 'W': return 2;             /* WARNING */
        case 'E': return 3;             /* ERROR */
        default:  return 1;
    }
}

static const char *sev_label(int sev)
{
    static const char *l[] = { "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL" };
    return (sev >= 0 && sev <= 4) ? l[sev] : "INFO";
}

static void timestamp_str(char *out, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    if (now > 1577836800) {   /* 2020-01-01 */
        localtime_r(&now, &tm);
        strftime(out, len, "%Y-%m-%d %H:%M:%S", &tm);
    } else {
        snprintf(out, len, "TIME_NOT_SYNCED");
    }
}

/* Emit one log line as "[timestamp] [SEVERITY] message\n", filtered. */
static void emit_line(char *line)
{
    size_t l = strlen(line);
    while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = '\0';
    if (l == 0) return;

    int sev = line_severity(line);
    if (sev < s_min_sev) return;

    char ts[24];
    timestamp_str(ts, sizeof(ts));
    char out[WS_LINE_MAX];
    int n = snprintf(out, sizeof(out), "[%s] [%s] %s\n", ts, sev_label(sev), line);
    if (n < 0) return;
    if (n >= (int)sizeof(out)) n = sizeof(out) - 1;
    broadcast(out, n);
}

/* ---- broadcast task ------------------------------------------------------ */

static void ws_task(void *arg)
{
    (void)arg;
    char *drain = malloc(WS_DRAIN_MAX + 1);
    char line[WS_LINE_MAX];
    size_t line_len = 0;
    while (s_run) {
        if (drain && !s_ota_suppress && s_client_count > 0) {
            size_t got = web_hook_logstream_read(drain, WS_DRAIN_MAX);
            for (size_t i = 0; i < got; i++) {
                char c = drain[i];
                if (c == '\n' || line_len >= sizeof(line) - 1) {
                    line[line_len] = '\0';
                    emit_line(line);
                    line_len = 0;
                } else {
                    line[line_len++] = c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    free(drain);
    s_task = NULL;
    vTaskDelete(NULL);
}

/* ---- replay to a newly connected client ---------------------------------- */

static void replay_buffered(int fd)
{
    const char *welcome = "[SYSTEM] Console connected. Monitoring serial output...\n";
    ws_send_text(fd, welcome, strlen(welcome));

    size_t total = web_hook_bootlog_size();
    if (total > WS_REPLAY_MAX) total = WS_REPLAY_MAX;
    if (total == 0) {
        const char *empty = "[SYSTEM] No buffered logs. Streaming live output...\n\n";
        ws_send_text(fd, empty, strlen(empty));
        return;
    }

    const char *hdr = "==== BUFFERED LOGS (Boot + Crash History) ====\n";
    ws_send_text(fd, hdr, strlen(hdr));

    char *chunk = malloc(WS_REPLAY_CHUNK + 1);
    if (chunk) {
        size_t off = 0;
        while (off < total) {
            size_t want = total - off;
            if (want > WS_REPLAY_CHUNK) want = WS_REPLAY_CHUNK;
            size_t got = web_hook_bootlog_read(off, chunk, want);
            if (got == 0) break;
            if (ws_send_text(fd, chunk, got) != ESP_OK) break;
            off += got;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        free(chunk);
    }
    const char *end = "==== END OF BUFFERED LOGS ====\n\n";
    ws_send_text(fd, end, strlen(end));
}

/* ---- handshake work item ------------------------------------------------- */

typedef struct { int fd; } ws_open_arg_t;

static void ws_open_work(void *arg)
{
    ws_open_arg_t *a = (ws_open_arg_t *)arg;
    clients_add(a->fd);
    replay_buffered(a->fd);
    free(a);
}

/* ---- HTTP handler -------------------------------------------------------- */

esp_err_t web_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* WebSocket handshake completed; schedule replay outside the handler. */
        int fd = httpd_req_to_sockfd(req);
        ws_open_arg_t *a = malloc(sizeof(*a));
        if (a) {
            a->fd = fd;
            if (httpd_queue_work(s_server, ws_open_work, a) != ESP_OK) { free(a); clients_add(fd); }
        }
        return ESP_OK;
    }

    /* Data frame: receive and discard (server never interprets client input). */
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK) return ret;
    if (frame.len && frame.len < 512) {
        uint8_t buf[512];
        frame.payload = buf;
        httpd_ws_recv_frame(req, &frame, sizeof(buf));
    }
    return ESP_OK;
}

/* ---- lifecycle ----------------------------------------------------------- */

esp_err_t web_ws_init(httpd_handle_t server)
{
    s_server = server;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    s_min_sev = (int)app_config_get_log_min_sev();
    s_client_count = 0;
    s_run = true;
    if (!s_task) {
        xTaskCreatePinnedToCore(ws_task, "web_ws", 4096, NULL, 4, &s_task, 0);
    }
    ESP_LOGI(TAG, "WebSocket log console ready at /ws/logs (min sev %d)", s_min_sev);
    return ESP_OK;
}

void web_ws_deinit(void)
{
    s_run = false;
    s_server = NULL;
    s_client_count = 0;
}

void web_ws_set_min_severity(int sev) { if (sev >= 0 && sev <= 4) s_min_sev = sev; }
void web_ws_set_ota_suppress(bool suppress) { s_ota_suppress = suppress; }
