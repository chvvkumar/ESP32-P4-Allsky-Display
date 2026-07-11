#include "mqtt_ha.h"
#include "app_config.h"
#include "config_debounce.h"
#include "display_defs.h"

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "driver/temperature_sensor.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <inttypes.h>

static const char *TAG = "mqtt_ha";

/* ---- Tunables ----------------------------------------------------------- */
#define MQTT_BUF_SIZE          2048     /* HA discovery payloads exceed 256 default */
#define MQTT_NET_TIMEOUT_MS    2000     /* socket timeout so a dead broker never blocks */
#define BACKOFF_INITIAL_MS     5000
#define BACKOFF_MAX_MS         60000
#define HEARTBEAT_PERIOD_US    (30LL * 1000000)
#define DISCOVERY_STEP_GAP_US  (50LL * 1000)
#define DISCOVERY_RETRY_US     (5LL * 1000000)   /* retry an aborted discovery (fix: OQ5) */
#define SERVICE_PERIOD_MS      50
#define CMD_QUEUE_LEN          8
#define CMD_PAYLOAD_MAX        257      /* 256 + NUL */
#define BRIGHT_SAVE_DEBOUNCE_MS 1500    /* coalesce brightness command bursts (fix: OQ8) */
#define ENTITY_COUNT           28

/* ---- Hooks -------------------------------------------------------------- */
static mqtt_ha_hooks_t s_hooks;

/* ---- Client / connection state ------------------------------------------ */
static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_connected = false;
static uint32_t s_backoff_ms = BACKOFF_INITIAL_MS;
static int s_fail_count = 0;
static int64_t s_next_attempt_us = 0;
static TaskHandle_t s_task = NULL;
static config_debounce_t s_bright_debounce;
static temperature_sensor_handle_t s_tsens = NULL;

/* ---- Discovery / publish scheduling ------------------------------------- */
static int s_disc_step = -1;         /* -1 idle; 0..27 in progress; ENTITY_COUNT = finalize */
static bool s_disc_failed = false;
static int64_t s_disc_last_us = 0;
static int64_t s_disc_retry_us = 0;
static int64_t s_last_hb_us = 0;
static int64_t s_last_sensor_us = 0;

/* ---- Identifiers / topics ----------------------------------------------- */
static char s_device_id[13];         /* 12 hex + NUL */
static char s_base[96];              /* <haStateTopic>/<deviceId> */
static char s_topic_avail[112];
static char s_topic_cmd_filter[112];

/* ================= Command model ========================================= */
typedef enum {
    CMD_BRIGHTNESS = 0, CMD_CYCLING, CMD_RANDOM, CMD_AUTOBRIGHT,
    CMD_CYCLE_INT, CMD_UPDATE_INT, CMD_IMAGE_SRC, CMD_REBOOT,
    CMD_NEXT, CMD_RESET_TF, CMD_UNKNOWN
} cmd_entity_t;

typedef struct {
    cmd_entity_t entity;
    int len;
    char payload[CMD_PAYLOAD_MAX];
} mqtt_cmd_t;

static QueueHandle_t s_cmd_queue = NULL;

/* Map a command entity id (second-to-last topic segment) to the enum. */
static cmd_entity_t entity_from_id(const char *id, int len)
{
    static const struct { const char *s; cmd_entity_t e; } M[] = {
        { "brightness", CMD_BRIGHTNESS }, { "cycling", CMD_CYCLING },
        { "random_order", CMD_RANDOM }, { "auto_brightness", CMD_AUTOBRIGHT },
        { "cycle_interval", CMD_CYCLE_INT }, { "update_interval", CMD_UPDATE_INT },
        { "image_source", CMD_IMAGE_SRC }, { "reboot", CMD_REBOOT },
        { "next_image", CMD_NEXT }, { "reset_transforms", CMD_RESET_TF },
    };
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) {
        if ((int)strlen(M[i].s) == len && strncmp(M[i].s, id, len) == 0) return M[i].e;
    }
    return CMD_UNKNOWN;
}

/* ================= Small helpers ========================================= */
static inline bool wifi_up(void)  { return s_hooks.wifi_connected ? s_hooks.wifi_connected() : true; }
static inline bool ota_busy(void) { return s_hooks.ota_in_progress ? s_hooks.ota_in_progress() : false; }

static int cur_brightness(void)
{
    if (s_hooks.get_brightness) return s_hooks.get_brightness();
    return app_config_get_def_bright();
}

static void device_ip(char *out, size_t len)
{
    out[0] = '\0';
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip;
        if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
            snprintf(out, len, IPSTR, IP2STR(&ip.ip));
        }
    }
    if (!out[0]) snprintf(out, len, "0.0.0.0");
}

static int8_t sta_rssi(void)
{
    int8_t v = 0;
    if (s_hooks.get_rssi) { s_hooks.get_rssi(&v); return v; }
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) v = ap.rssi;
    return v;
}

static float soc_temp_c(void)
{
    if (!s_tsens) {
        temperature_sensor_config_t c = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
        if (temperature_sensor_install(&c, &s_tsens) != ESP_OK) { s_tsens = NULL; return NAN; }
        temperature_sensor_enable(s_tsens);
    }
    float t = NAN;
    if (temperature_sensor_get_celsius(s_tsens, &t) != ESP_OK) return NAN;
    return t;
}

static bool pub(const char *topic, const char *payload, int retain)
{
    if (!s_client || !s_connected) return false;
    return esp_mqtt_client_publish(s_client, topic, payload, 0, 0, retain) != -1;
}

/* <base>/<id>/<suffix> into buf. */
static void topic_of(char *buf, size_t len, const char *id, const char *suffix)
{
    snprintf(buf, len, "%s/%s/%s", s_base, id, suffix);
}

static void publish_echo(const char *id, const char *payload)
{
    char t[112];
    topic_of(t, sizeof(t), id, "state");
    pub(t, payload, 1);  /* retained state (fix: OQ6) */
}

/* ================= Device object ========================================= */
static cJSON *make_device(const char *device_name)
{
    cJSON *dev = cJSON_CreateObject();
    cJSON *ids = cJSON_CreateArray();
    cJSON_AddItemToArray(ids, cJSON_CreateString(s_device_id));
    cJSON_AddItemToObject(dev, "identifiers", ids);
    cJSON_AddStringToObject(dev, "name", device_name);
    cJSON_AddStringToObject(dev, "model", "ESP32-P4-WIFI6-Touch-LCD");
    cJSON_AddStringToObject(dev, "manufacturer", "chvvkumar");
    const esp_app_desc_t *d = esp_app_get_description();  /* real firmware version (fix: OQ3) */
    cJSON_AddStringToObject(dev, "sw_version", d->version);
    char ip[16], url[40];
    device_ip(ip, sizeof(ip));
    snprintf(url, sizeof(url), "http://%s:8080", ip);
    cJSON_AddStringToObject(dev, "configuration_url", url);
    return dev;
}

/* ================= Discovery catalog ===================================== */
typedef enum { C_LIGHT, C_SWITCH, C_NUMBER, C_SELECT, C_BUTTON, C_SENSOR } comp_t;
typedef struct {
    comp_t       comp;
    const char  *id;
    const char  *name;       /* entity name suffix; device_name is prepended */
    const char  *icon;
    const char  *unit;       /* NULL if none */
    const char  *dev_class;  /* NULL if none */
    int          nmin, nmax, nstep;  /* numbers only */
} ent_t;

/* Number ranges reconciled to what the command handler accepts (fix: OQ4). */
static const ent_t CATALOG[ENTITY_COUNT] = {
    { C_LIGHT,  "brightness",      "Brightness",       "mdi:brightness-6",       NULL, NULL, 0,0,0 },
    { C_SWITCH, "cycling",         "Cycling Enabled",  "mdi:image-multiple",     NULL, NULL, 0,0,0 },
    { C_SWITCH, "random_order",    "Random Order",     "mdi:shuffle",            NULL, NULL, 0,0,0 },
    { C_SWITCH, "auto_brightness", "Auto Brightness",  "mdi:brightness-auto",    NULL, NULL, 0,0,0 },
    { C_NUMBER, "cycle_interval",  "Cycle Interval",   "mdi:timer",              "s",  NULL, 10,3600,10 },
    { C_NUMBER, "update_interval", "Update Interval",  "mdi:update",             "s",  NULL, 10,3600,10 },
    { C_SELECT, "image_source",    "Image Source",     "mdi:image-multiple",     NULL, NULL, 0,0,0 },
    { C_BUTTON, "reboot",          "Reboot",           "mdi:restart",            NULL, NULL, 0,0,0 },
    { C_BUTTON, "next_image",      "Next Image",       "mdi:skip-next",          NULL, NULL, 0,0,0 },
    { C_BUTTON, "reset_transforms","Reset Transforms", "mdi:restore",            NULL, NULL, 0,0,0 },
    { C_SENSOR, "current_image",           "Current Image URL",   "mdi:image",              NULL, NULL, 0,0,0 },
    { C_SENSOR, "free_heap",               "Free Heap",           "mdi:memory",             "KB", NULL, 0,0,0 },
    { C_SENSOR, "free_psram",              "Free PSRAM",          "mdi:memory",             "KB", NULL, 0,0,0 },
    { C_SENSOR, "wifi_rssi",               "WiFi Signal",         "mdi:wifi",               "dBm","signal_strength", 0,0,0 },
    { C_SENSOR, "uptime",                  "Uptime",              "mdi:clock-outline",      "s",  "duration", 0,0,0 },
    { C_SENSOR, "uptime_readable",         "Uptime Readable",     "mdi:clock-outline",      NULL, NULL, 0,0,0 },
    { C_SENSOR, "image_count",             "Image Count",         "mdi:counter",            NULL, NULL, 0,0,0 },
    { C_SENSOR, "current_image_index",     "Current Image Index", "mdi:numeric",            NULL, NULL, 0,0,0 },
    { C_SENSOR, "cycling_mode",            "Cycling Mode",        "mdi:sync",               NULL, NULL, 0,0,0 },
    { C_SENSOR, "random_order_status",     "Random Order",        "mdi:shuffle-variant",    NULL, NULL, 0,0,0 },
    { C_SENSOR, "cycle_interval_status",   "Cycle Interval",      "mdi:timer-outline",      "s",  NULL, 0,0,0 },
    { C_SENSOR, "update_interval_status",  "Update Interval",     "mdi:update",             "s",  NULL, 0,0,0 },
    { C_SENSOR, "display_width",           "Display Width",       "mdi:monitor-screenshot", "px", NULL, 0,0,0 },
    { C_SENSOR, "display_height",          "Display Height",      "mdi:monitor-screenshot", "px", NULL, 0,0,0 },
    { C_SENSOR, "auto_brightness_status",  "Auto Brightness",     "mdi:brightness-auto",    NULL, NULL, 0,0,0 },
    { C_SENSOR, "brightness_level",        "Brightness Level",    "mdi:brightness-6",       "%",  NULL, 0,0,0 },
    { C_SENSOR, "temperature_celsius",     "Temperature",         "mdi:thermometer",        "\xC2\xB0" "C", "temperature", 0,0,0 },
    { C_SENSOR, "temperature_fahrenheit",  "Temperature (F)",     "mdi:thermometer",        "\xC2\xB0" "F", "temperature", 0,0,0 },
};

static const char *comp_name(comp_t c)
{
    switch (c) {
        case C_LIGHT:  return "light";
        case C_SWITCH: return "switch";
        case C_NUMBER: return "number";
        case C_SELECT: return "select";
        case C_BUTTON: return "button";
        default:       return "sensor";
    }
}

/* Publish one discovery config message. Returns true on publish success. */
static bool publish_discovery_entity(int step, const allsky_config_t *cfg)
{
    const ent_t *e = &CATALOG[step];
    char st[112], ct[112];
    topic_of(st, sizeof(st), e->id, "state");
    topic_of(ct, sizeof(ct), e->id, "set");

    cJSON *o = cJSON_CreateObject();

    char name[96], uid[48];
    snprintf(name, sizeof(name), "%s %s", cfg->device_name, e->name);
    snprintf(uid, sizeof(uid), "%s_%s", s_device_id, e->id);
    cJSON_AddStringToObject(o, "name", name);
    cJSON_AddStringToObject(o, "unique_id", uid);
    cJSON_AddStringToObject(o, "availability_topic", s_topic_avail);
    cJSON_AddStringToObject(o, "icon", e->icon);
    cJSON_AddItemToObject(o, "device", make_device(cfg->device_name));

    switch (e->comp) {
        case C_LIGHT:
            cJSON_AddStringToObject(o, "state_topic", st);
            cJSON_AddStringToObject(o, "command_topic", ct);
            cJSON_AddStringToObject(o, "brightness_state_topic", st);
            cJSON_AddStringToObject(o, "brightness_command_topic", ct);
            cJSON_AddNumberToObject(o, "brightness_scale", 100);
            cJSON_AddStringToObject(o, "on_command_type", "brightness");
            break;
        case C_SWITCH:
            cJSON_AddStringToObject(o, "state_topic", st);
            cJSON_AddStringToObject(o, "command_topic", ct);
            cJSON_AddStringToObject(o, "payload_on", "ON");
            cJSON_AddStringToObject(o, "payload_off", "OFF");
            break;
        case C_NUMBER: {
            cJSON_AddStringToObject(o, "state_topic", st);
            cJSON_AddStringToObject(o, "command_topic", ct);
            cJSON_AddNumberToObject(o, "min", e->nmin);
            cJSON_AddNumberToObject(o, "max", e->nmax);
            char step_s[16];
            snprintf(step_s, sizeof(step_s), "%d.00", e->nstep);
            cJSON_AddRawToObject(o, "step", step_s);
            if (e->unit) cJSON_AddStringToObject(o, "unit_of_measurement", e->unit);
            break;
        }
        case C_SELECT: {
            cJSON_AddStringToObject(o, "state_topic", st);
            cJSON_AddStringToObject(o, "command_topic", ct);
            cJSON *opts = cJSON_CreateArray();
            int n = cfg->img_src_cnt;
            for (int i = 1; i <= n; i++) {
                char lbl[16];
                snprintf(lbl, sizeof(lbl), "Image %d", i);
                cJSON_AddItemToArray(opts, cJSON_CreateString(lbl));
            }
            cJSON_AddItemToObject(o, "options", opts);
            break;
        }
        case C_BUTTON:
            cJSON_AddStringToObject(o, "command_topic", ct);
            cJSON_AddStringToObject(o, "payload_press", "PRESS");
            break;
        case C_SENSOR:
            cJSON_AddStringToObject(o, "state_topic", st);
            if (e->unit)      cJSON_AddStringToObject(o, "unit_of_measurement", e->unit);
            if (e->dev_class) cJSON_AddStringToObject(o, "device_class", e->dev_class);
            break;
    }

    char disc_topic[176];
    snprintf(disc_topic, sizeof(disc_topic), "%s/%s/%s/%s/config",
             cfg->ha_disc_pfx, comp_name(e->comp), s_device_id, e->id);

    bool ok = false;
    char *payload = cJSON_PrintUnformatted(o);
    if (payload) {
        ok = esp_mqtt_client_publish(s_client, disc_topic, payload, 0, 1, 1) != -1;
        free(payload);
    }
    cJSON_Delete(o);
    return ok;
}

/* ================= State + sensor publishes ============================== */
static void uptime_readable(char *out, size_t len, int64_t sec)
{
    int64_t d = sec / 86400; int64_t h = (sec % 86400) / 3600;
    int64_t m = (sec % 3600) / 60; int64_t s = sec % 60;
    if (d > 0)      snprintf(out, len, "%lldd %lldh %lldm", (long long)d, (long long)h, (long long)m);
    else if (h > 0) snprintf(out, len, "%lldh %lldm", (long long)h, (long long)m);
    else            snprintf(out, len, "%lldm %llds", (long long)m, (long long)s);
}

static void publish_sensors_locked(const allsky_config_t *cfg)
{
    char t[112], v[272];
    int64_t up = esp_timer_get_time() / 1000000;
    allsky_panel_profile_t panel = allsky_panel_profile();
    int brightness = cur_brightness();

    char url[256];
    app_config_current_image_url(url, sizeof(url));
    topic_of(t, sizeof(t), "current_image", "state"); pub(t, url, 1);

    size_t heap_kb  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t psram_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
    topic_of(t, sizeof(t), "free_heap", "state");  snprintf(v, sizeof(v), "%u", (unsigned)heap_kb);  pub(t, v, 1);
    topic_of(t, sizeof(t), "free_psram", "state"); snprintf(v, sizeof(v), "%u", (unsigned)psram_kb); pub(t, v, 1);

    topic_of(t, sizeof(t), "wifi_rssi", "state"); snprintf(v, sizeof(v), "%d", sta_rssi()); pub(t, v, 1);

    topic_of(t, sizeof(t), "uptime", "state"); snprintf(v, sizeof(v), "%lld", (long long)up); pub(t, v, 1);
    topic_of(t, sizeof(t), "uptime_readable", "state"); uptime_readable(v, sizeof(v), up); pub(t, v, 1);

    topic_of(t, sizeof(t), "image_count", "state"); snprintf(v, sizeof(v), "%d", cfg->img_src_cnt); pub(t, v, 1);
    topic_of(t, sizeof(t), "current_image_index", "state"); snprintf(v, sizeof(v), "%ld", (long)cfg->curr_img_idx + 1); pub(t, v, 1);

    topic_of(t, sizeof(t), "cycling_mode", "state"); pub(t, cfg->cycling_en ? "Cycling" : "Single", 1);
    topic_of(t, sizeof(t), "random_order_status", "state"); pub(t, cfg->random_ord ? "Enabled" : "Disabled", 1);

    topic_of(t, sizeof(t), "cycle_interval_status", "state"); snprintf(v, sizeof(v), "%lu", (unsigned long)(cfg->cycle_intv / 1000)); pub(t, v, 1);
    topic_of(t, sizeof(t), "update_interval_status", "state"); snprintf(v, sizeof(v), "%lu", (unsigned long)(cfg->upd_interval / 1000)); pub(t, v, 1);

    topic_of(t, sizeof(t), "display_width", "state"); snprintf(v, sizeof(v), "%u", panel.width); pub(t, v, 1);
    topic_of(t, sizeof(t), "display_height", "state"); snprintf(v, sizeof(v), "%u", panel.height); pub(t, v, 1);

    topic_of(t, sizeof(t), "auto_brightness_status", "state"); pub(t, cfg->bright_auto ? "Enabled" : "Disabled", 1);
    topic_of(t, sizeof(t), "brightness_level", "state"); snprintf(v, sizeof(v), "%d", brightness); pub(t, v, 1);

    float c = soc_temp_c();
    if (!isnan(c)) {
        topic_of(t, sizeof(t), "temperature_celsius", "state"); snprintf(v, sizeof(v), "%.1f", c); pub(t, v, 1);
        topic_of(t, sizeof(t), "temperature_fahrenheit", "state"); snprintf(v, sizeof(v), "%.1f", c * 9.0f / 5.0f + 32.0f); pub(t, v, 1);
    }
}

static void publish_state_and_sensors(void)
{
    if (!s_connected || !app_config_get_ha_disc_en()) return;
    /* Snapshot on the heap: allsky_config_t is several KB (image source array),
     * too large to place on a service task stack alongside newlib float formatting. */
    allsky_config_t *cfg = heap_caps_malloc(sizeof(*cfg), MALLOC_CAP_DEFAULT);
    if (!cfg) return;
    app_config_snapshot(cfg);

    char t[112], v[16];
    topic_of(t, sizeof(t), "brightness", "state"); snprintf(v, sizeof(v), "%d", cur_brightness()); pub(t, v, 1);
    topic_of(t, sizeof(t), "cycling", "state"); pub(t, cfg->cycling_en ? "ON" : "OFF", 1);
    topic_of(t, sizeof(t), "random_order", "state"); pub(t, cfg->random_ord ? "ON" : "OFF", 1);
    topic_of(t, sizeof(t), "auto_brightness", "state"); pub(t, cfg->bright_auto ? "ON" : "OFF", 1);
    topic_of(t, sizeof(t), "cycle_interval", "state"); snprintf(v, sizeof(v), "%lu", (unsigned long)(cfg->cycle_intv / 1000)); pub(t, v, 1);
    topic_of(t, sizeof(t), "update_interval", "state"); snprintf(v, sizeof(v), "%lu", (unsigned long)(cfg->upd_interval / 1000)); pub(t, v, 1);
    topic_of(t, sizeof(t), "image_source", "state"); snprintf(v, sizeof(v), "Image %ld", (long)cfg->curr_img_idx + 1); pub(t, v, 1);

    publish_sensors_locked(cfg);
    free(cfg);
}

void mqtt_ha_publish_state(void) { publish_state_and_sensors(); }

/* ================= Command apply (service-task context) ================== */
static void bright_flush_cb(void *arg) { (void)arg; app_config_save(); }

static void apply_command(const mqtt_cmd_t *c)
{
    const char *p = c->payload;
    bool is_press = (c->len >= 5 && strncmp(p, "PRESS", 5) == 0);
    bool is_on    = (strncmp(p, "ON", 2) == 0 && (p[2] == '\0'));

    switch (c->entity) {
    case CMD_BRIGHTNESS: {
        if (app_config_get_use_ha_rest()) {
            ESP_LOGI(TAG, "brightness command ignored: HA REST control active");
            break;
        }
        char *end = NULL;
        long val = strtol(p, &end, 10);
        if (end == p) val = 0;               /* non-numeric parses to 0 */
        if (val < 0 || val > 100) { ESP_LOGW(TAG, "brightness %ld out of range, dropped", val); break; }
        if (s_hooks.apply_brightness) s_hooks.apply_brightness((int)val);
        app_config_set_def_bright((int32_t)val);
        config_debounce_touch(&s_bright_debounce);   /* coalesced NVS save (fix: OQ8) */
        publish_echo("brightness", p);
        break;
    }
    case CMD_CYCLING:
        app_config_set_cycling_en(is_on); app_config_save(); publish_echo("cycling", p);
        break;
    case CMD_RANDOM:
        app_config_set_random_ord(is_on); app_config_save(); publish_echo("random_order", p);
        break;
    case CMD_AUTOBRIGHT:
        app_config_set_bright_auto(is_on);
        if (is_on) app_config_set_use_ha_rest(false);   /* mutually exclusive; MQTT wins */
        app_config_save(); publish_echo("auto_brightness", p);
        break;
    case CMD_CYCLE_INT: {
        long s = strtol(p, NULL, 10);
        if (s < 10 || s > 3600) { ESP_LOGW(TAG, "cycle_interval %ld out of range", s); break; }
        app_config_set_cycle_intv((uint32_t)(s * 1000)); app_config_save(); publish_echo("cycle_interval", p);
        break;
    }
    case CMD_UPDATE_INT: {
        long s = strtol(p, NULL, 10);
        if (s < 10 || s > 3600) { ESP_LOGW(TAG, "update_interval %ld out of range", s); break; }
        app_config_set_upd_interval((uint32_t)(s * 1000)); app_config_save(); publish_echo("update_interval", p);
        break;
    }
    case CMD_IMAGE_SRC: {
        if (c->len < 7) break;
        long n = strtol(p + 6, NULL, 10);   /* "Image N": N begins at offset 6 */
        int idx = (int)n - 1;
        if (idx >= 0 && idx < app_config_source_count()) {
            app_config_set_curr_img_idx(idx); app_config_save();
            if (s_hooks.image_index_changed) s_hooks.image_index_changed(idx);
            publish_echo("image_source", p);
        }
        break;
    }
    case CMD_NEXT: {
        if (!is_press) break;
        int count = app_config_source_count();
        if (count <= 0) break;
        int idx = ((int)app_config_get_curr_img_idx() + 1) % count;
        app_config_set_curr_img_idx(idx); app_config_save();
        if (s_hooks.image_index_changed) s_hooks.image_index_changed(idx);
        char t[112], v[16];
        topic_of(t, sizeof(t), "image_source", "state");
        snprintf(v, sizeof(v), "Image %d", idx + 1);
        pub(t, v, 1);
        break;
    }
    case CMD_RESET_TF: {
        if (!is_press) break;
        image_transform_t def = {
            .scale_x = app_config_get_def_scale_x(), .scale_y = app_config_get_def_scale_y(),
            .offset_x = app_config_get_def_off_x(), .offset_y = app_config_get_def_off_y(),
            .rotation = app_config_get_def_rot(),
        };
        int count = app_config_source_count();
        for (int i = 0; i < count; i++) app_config_set_source_transform(i, &def);
        app_config_save();
        publish_state_and_sensors();
        break;
    }
    case CMD_REBOOT:
        if (!is_press) break;
        ESP_LOGW(TAG, "reboot requested via MQTT");
        config_debounce_flush_now(&s_bright_debounce);
        if (s_hooks.before_reboot) s_hooks.before_reboot();
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_restart();
        break;
    default:
        break;
    }
}

static void drain_commands(void)
{
    if (!s_cmd_queue) return;
    mqtt_cmd_t c;
    while (xQueueReceive(s_cmd_queue, &c, 0) == pdTRUE) apply_command(&c);
}

/* ================= MQTT event handler (parse + enqueue only) ============= */
static void enqueue_from_topic(const char *topic, int topic_len, const char *data, int data_len)
{
    /* topic shape: <base>/<entity>/set — entity is the second-to-last segment. */
    int base_len = (int)strlen(s_base);
    if (topic_len <= base_len + 5) return;                 /* need "/x/set" at least */
    if (strncmp(topic, s_base, base_len) != 0) return;
    if (strncmp(topic + topic_len - 4, "/set", 4) != 0) return;
    const char *ent = topic + base_len + 1;                /* skip "<base>/" */
    int ent_len = (topic_len - 4) - (base_len + 1);        /* up to "/set" */
    if (ent_len <= 0) return;

    cmd_entity_t e = entity_from_id(ent, ent_len);
    if (e == CMD_UNKNOWN) return;

    if (data_len > 256) { ESP_LOGW(TAG, "payload %d bytes > 256, dropped", data_len); return; }

    mqtt_cmd_t cmd = { .entity = e, .len = data_len };
    int copy = data_len < CMD_PAYLOAD_MAX - 1 ? data_len : CMD_PAYLOAD_MAX - 1;
    memcpy(cmd.payload, data, copy);
    cmd.payload[copy] = '\0';
    if (xQueueSend(s_cmd_queue, &cmd, 0) != pdTRUE) ESP_LOGW(TAG, "command queue full, dropped");
}

static void mqtt_event_handler(void *args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)args; (void)base;
    esp_mqtt_event_handle_t ev = event_data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected to broker");
        s_connected = true;
        s_fail_count = 0;
        s_backoff_ms = BACKOFF_INITIAL_MS;
        pub(s_topic_avail, "online", 1);
        s_last_hb_us = esp_timer_get_time();
        if (app_config_get_ha_disc_en()) {
            s_disc_step = 0;          /* start non-blocking discovery */
            s_disc_failed = false;
            s_disc_last_us = 0;
        } else {
            s_disc_step = -1;
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected from broker");
        s_connected = false;
        s_disc_step = -1;
        s_fail_count++;
        if (s_fail_count > 3) {               /* grow backoff only after >3 failures */
            uint32_t next = (uint32_t)(s_backoff_ms * 1.5f);
            s_backoff_ms = next > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : next;
        }
        break;
    case MQTT_EVENT_DATA:
        /* Reject oversized payloads before any copy; discovery-gated routing. */
        if (ev->total_data_len > 1024) { ESP_LOGW(TAG, "oversized MQTT payload dropped"); break; }
        if (!app_config_get_ha_disc_en()) break;
        if (ev->topic && ev->topic_len > 0)
            enqueue_from_topic(ev->topic, ev->topic_len, ev->data, ev->data_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error event");
        break;
    default:
        break;
    }
}

/* ================= Reconnect ============================================= */
/* Regenerate the client ID (random hex suffix) to avoid stale-session kickoff. */
static void refresh_client_id(void)
{
    if (!s_client) return;
    char cid_base[48];
    app_config_get_mqtt_client(cid_base, sizeof(cid_base));
    char cid[64];
    snprintf(cid, sizeof(cid), "%s_%x", cid_base, (unsigned)(esp_random() & 0xFFFF));
    esp_mqtt_client_config_t cfg = { .credentials.client_id = cid };
    esp_mqtt_set_config(s_client, &cfg);
}

static void try_reconnect(void)
{
    int64_t now = esp_timer_get_time();
    if (now < s_next_attempt_us) return;
    uint32_t floor_ms = app_config_get_mqtt_recon();       /* wired mqttReconnectInterval (fix: OQ2) */
    uint32_t gap = s_backoff_ms > floor_ms ? s_backoff_ms : floor_ms;
    s_next_attempt_us = now + (int64_t)gap * 1000;
    ESP_LOGI(TAG, "reconnect attempt (backoff %" PRIu32 " ms)", gap);
    if (s_hooks.on_reconnect_attempt) s_hooks.on_reconnect_attempt();
    refresh_client_id();
    esp_mqtt_client_reconnect(s_client);
}

/* ================= Discovery / heartbeat scheduling ====================== */
static void advance_discovery(void)
{
    if (!app_config_get_ha_disc_en()) return;

    /* Retry an aborted sequence while still connected (fix: OQ5). */
    if (s_disc_failed && s_disc_step < 0) {
        int64_t now = esp_timer_get_time();
        if (now >= s_disc_retry_us) { s_disc_step = 0; s_disc_failed = false; s_disc_last_us = 0; }
        return;
    }
    if (s_disc_step < 0) return;

    int64_t now = esp_timer_get_time();
    if (now - s_disc_last_us < DISCOVERY_STEP_GAP_US) return;

    if (s_disc_step < ENTITY_COUNT) {
        /* Heap snapshot: keep the multi-KB config off this task's stack, and free it
         * before publish_state_and_sensors() so the two snapshots never layer. */
        allsky_config_t *cfg = heap_caps_malloc(sizeof(*cfg), MALLOC_CAP_DEFAULT);
        if (!cfg) return;
        app_config_snapshot(cfg);
        bool ok = publish_discovery_entity(s_disc_step, cfg);
        free(cfg);
        if (!ok) {
            ESP_LOGW(TAG, "discovery step %d failed, aborting", s_disc_step);
            s_disc_step = -1;
            s_disc_failed = true;
            s_disc_retry_us = now + DISCOVERY_RETRY_US;
            return;
        }
        s_disc_last_us = now;
        s_disc_step++;
        if (s_disc_step == ENTITY_COUNT) {
            esp_mqtt_client_subscribe(s_client, s_topic_cmd_filter, 1);
            publish_state_and_sensors();
            s_last_sensor_us = now;
            s_disc_step = -1;
            ESP_LOGI(TAG, "HA discovery complete (%d entities), subscribed to %s",
                     ENTITY_COUNT, s_topic_cmd_filter);
        }
    }
}

static void service_heartbeat_and_sensors(void)
{
    int64_t now = esp_timer_get_time();
    if (now - s_last_hb_us >= HEARTBEAT_PERIOD_US) {
        pub(s_topic_avail, "online", 1);
        s_last_hb_us = now;
    }
    if (app_config_get_ha_disc_en() && s_disc_step < 0 && !s_disc_failed) {
        int64_t period = (int64_t)app_config_get_ha_sens_int() * 1000000;
        if (now - s_last_sensor_us >= period) {
            allsky_config_t *cfg = heap_caps_malloc(sizeof(*cfg), MALLOC_CAP_DEFAULT);
            if (cfg) {
                app_config_snapshot(cfg);
                publish_sensors_locked(cfg);
                free(cfg);
                s_last_sensor_us = now;
            }
        }
    }
}

/* ================= Service task ========================================== */
static void mqtt_ha_task(void *arg)
{
    (void)arg;
    for (;;) {
        drain_commands();
        if (ota_busy()) { vTaskDelay(pdMS_TO_TICKS(SERVICE_PERIOD_MS)); continue; }

        if (s_client) {
            if (!s_connected) {
                if (wifi_up()) try_reconnect();
            } else {
                advance_discovery();
                service_heartbeat_and_sensors();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SERVICE_PERIOD_MS));
    }
}

/* ================= Identifiers / topics init ============================= */
static void init_device_id(void)
{
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK &&
        esp_read_mac(mac, ESP_MAC_BASE) != ESP_OK) {
        snprintf(s_device_id, sizeof(s_device_id), "000000000000");
        return;
    }
    snprintf(s_device_id, sizeof(s_device_id), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void build_topics(void)
{
    char root[48];
    app_config_get_ha_state_top(root, sizeof(root));
    snprintf(s_base, sizeof(s_base), "%s/%s", root, s_device_id);
    snprintf(s_topic_avail, sizeof(s_topic_avail), "%s/availability", s_base);
    snprintf(s_topic_cmd_filter, sizeof(s_topic_cmd_filter), "%s/+/set", s_base);
}

/* ================= Public API ============================================ */
esp_err_t mqtt_ha_init(const mqtt_ha_hooks_t *hooks)
{
    if (hooks) s_hooks = *hooks; else memset(&s_hooks, 0, sizeof(s_hooks));

    init_device_id();
    build_topics();

    if (!s_cmd_queue) {
        s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(mqtt_cmd_t));
        if (!s_cmd_queue) { ESP_LOGE(TAG, "command queue alloc failed"); return ESP_ERR_NO_MEM; }
    }

    esp_err_t err = config_debounce_init(&s_bright_debounce, BRIGHT_SAVE_DEBOUNCE_MS, bright_flush_cb, NULL);
    if (err != ESP_OK) { ESP_LOGE(TAG, "brightness debounce init failed: %s", esp_err_to_name(err)); return err; }

    if (!s_task) {
        if (xTaskCreate(mqtt_ha_task, "mqtt_ha", 8192, NULL, 4, &s_task) != pdPASS) {
            ESP_LOGE(TAG, "service task create failed");
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG, "initialized (device_id=%s, base=%s)", s_device_id, s_base);
    return ESP_OK;
}

esp_err_t mqtt_ha_start(void)
{
    char server[64];
    app_config_get_mqtt_server(server, sizeof(server));
    if (server[0] == '\0') { ESP_LOGI(TAG, "no broker configured, MQTT idle"); return ESP_OK; }

    if (s_client) { ESP_LOGW(TAG, "client already running"); return ESP_OK; }

    char user[48], pwd[64], client_base[48];
    app_config_get_mqtt_user(user, sizeof(user));
    app_config_get_mqtt_pwd(pwd, sizeof(pwd));
    app_config_get_mqtt_client(client_base, sizeof(client_base));

    char uri[96];
    if (strncmp(server, "mqtt://", 7) == 0 || strncmp(server, "mqtts://", 8) == 0)
        snprintf(uri, sizeof(uri), "%s", server);
    else
        snprintf(uri, sizeof(uri), "mqtt://%s", server);

    char cid[64];
    snprintf(cid, sizeof(cid), "%s_%x", client_base, (unsigned)(esp_random() & 0xFFFF));

    bool have_user = user[0] != '\0';
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .broker.address.port = (uint32_t)app_config_get_mqtt_port(),
        .credentials.client_id = cid,
        .credentials.username = have_user ? user : NULL,
        .credentials.authentication.password = (have_user && pwd[0]) ? pwd : NULL,
        .session.last_will.topic = s_topic_avail,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .network.disable_auto_reconnect = true,
        .network.timeout_ms = MQTT_NET_TIMEOUT_MS,
        .buffer.size = MQTT_BUF_SIZE,
        .buffer.out_size = MQTT_BUF_SIZE,
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) { ESP_LOGE(TAG, "client init failed"); return ESP_FAIL; }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "client start failed: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return err;
    }
    s_backoff_ms = BACKOFF_INITIAL_MS;
    s_fail_count = 0;
    s_next_attempt_us = 0;
    ESP_LOGI(TAG, "client started -> %s:%ld", uri, (long)app_config_get_mqtt_port());
    return ESP_OK;
}

void mqtt_ha_stop(void)
{
    if (!s_client) return;
    if (s_connected) pub(s_topic_avail, "offline", 1);
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;
    s_disc_step = -1;
    ESP_LOGI(TAG, "client stopped");
}

bool mqtt_ha_is_connected(void) { return s_connected; }

void mqtt_ha_notify_sources_changed(void)
{
    /* Rebuild the select option list and republish the retained set (fix: OQ7). */
    if (s_connected && app_config_get_ha_disc_en()) {
        s_disc_step = 0;
        s_disc_failed = false;
        s_disc_last_us = 0;
        ESP_LOGI(TAG, "source list changed, re-running discovery");
    }
}

void mqtt_ha_force_reconnect(void)
{
    if (!s_client) return;
    ESP_LOGI(TAG, "forced MQTT reconnect (serial)");
    s_connected = false;
    s_next_attempt_us = 0;
    s_backoff_ms = BACKOFF_INITIAL_MS;
    esp_mqtt_client_disconnect(s_client);
}

void mqtt_ha_connection_info(char *out, size_t len)
{
    char server[64], client_base[48];
    app_config_get_mqtt_server(server, sizeof(server));
    app_config_get_mqtt_client(client_base, sizeof(client_base));
    int port = app_config_get_mqtt_port();
    if (s_connected && app_config_get_ha_disc_en()) {
        char dev[48];
        app_config_get_ha_dev_name(dev, sizeof(dev));
        snprintf(out, len, "MQTT: connected %s:%d client=%s device=\"%s\" cmd_filter=%s",
                 server, port, client_base, dev, s_topic_cmd_filter);
    } else {
        snprintf(out, len, "MQTT: %s %s:%d client=%s",
                 s_connected ? "connected" : "disconnected", server, port, client_base);
    }
}

/* ================= HA REST ambient-light brightness client =============== */
static float s_rest_last_lux = NAN;
static int   s_rest_last_bright = -1;

bool mqtt_ha_rest_last(float *lux, int *brightness)
{
    if (s_rest_last_bright < 0) return false;
    if (lux) *lux = s_rest_last_lux;
    if (brightness) *brightness = s_rest_last_bright;
    return true;
}

/* Accumulate the HTTP body into a growable buffer via the event callback. */
typedef struct { char *buf; int len; int cap; } rest_body_t;

static esp_err_t rest_http_evt(esp_http_client_event_t *e)
{
    if (e->event_id == HTTP_EVENT_ON_DATA) {
        rest_body_t *b = (rest_body_t *)e->user_data;
        if (!b) return ESP_OK;
        int need = b->len + e->data_len + 1;
        if (need > b->cap) {
            int ncap = need < 512 ? 512 : need;
            char *n = realloc(b->buf, ncap);
            if (!n) return ESP_FAIL;
            b->buf = n; b->cap = ncap;
        }
        memcpy(b->buf + b->len, e->data, e->data_len);
        b->len += e->data_len;
        b->buf[b->len] = '\0';
    }
    return ESP_OK;
}

static int lux_to_brightness(float lux)
{
    float min_lux = app_config_get_sensor_min_lux();
    float max_lux = app_config_get_sensor_max_lux();
    int min_b = app_config_get_disp_min_br();
    int max_b = app_config_get_disp_max_br();
    int mode  = app_config_get_sensor_map_mode();

    if (lux < min_lux) lux = min_lux;
    if (lux > max_lux) lux = max_lux;

    float b;
    switch (mode) {
    case 0: {  /* linear */
        float range = max_lux - min_lux;
        b = (range <= 0.0f) ? (float)min_b
                            : min_b + (lux - min_lux) / range * (max_b - min_b);
        break;
    }
    case 2: {  /* threshold day/night */
        float mid = (min_lux + max_lux) / 2.0f;
        b = (lux >= mid) ? (float)max_b : (float)min_b;
        break;
    }
    case 1:
    default: {  /* logarithmic (default) */
        if (mode != 1) ESP_LOGE(TAG, "unknown mapping mode %d, using linear", mode);
        if (mode != 1) {
            float range = max_lux - min_lux;
            b = (range <= 0.0f) ? (float)min_b
                                : min_b + (lux - min_lux) / range * (max_b - min_b);
        } else {
            float lr = log10f(max_lux + 1.0f) - log10f(min_lux + 1.0f);
            float ratio = (lr <= 0.0f) ? 0.0f
                          : (log10f(lux + 1.0f) - log10f(min_lux + 1.0f)) / lr;
            b = min_b + ratio * (max_b - min_b);
        }
        break;
    }
    }
    int bi = (int)(b + 0.5f);
    if (bi < 0) bi = 0;
    if (bi > 100) bi = 100;
    return bi;
}

static void rest_poll_once(void)
{
    char base[128], token[256], ent[96];
    app_config_get_ha_base_url(base, sizeof(base));
    app_config_get_ha_token(token, sizeof(token));
    app_config_get_ha_sensor_ent(ent, sizeof(ent));
    if (!base[0] || !token[0] || !ent[0]) { ESP_LOGW(TAG, "HA REST: base/token/entity empty, skipping"); return; }

    char url[256];
    snprintf(url, sizeof(url), "%s/api/states/%s", base, ent);
    char auth[300];
    snprintf(auth, sizeof(auth), "Bearer %s", token);

    rest_body_t body = { 0 };
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = 5000,
        .event_handler = rest_http_evt,
        .user_data = &body,
        .crt_bundle_attach = NULL,
    };
    esp_http_client_handle_t cli = esp_http_client_init(&cfg);
    if (!cli) return;
    esp_http_client_set_header(cli, "Authorization", auth);

    esp_err_t err = esp_http_client_perform(cli);
    int status = esp_http_client_get_status_code(cli);
    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "HA REST poll failed: err=%s status=%d", esp_err_to_name(err), status);
        esp_http_client_cleanup(cli);
        free(body.buf);
        return;
    }
    esp_http_client_cleanup(cli);

    cJSON *root = body.buf ? cJSON_Parse(body.buf) : NULL;
    free(body.buf);
    if (!root) { ESP_LOGW(TAG, "HA REST: JSON parse failed"); return; }

    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (cJSON_IsString(state) && state->valuestring) {
        if (strcmp(state->valuestring, "unavailable") != 0 &&
            strcmp(state->valuestring, "unknown") != 0) {
            float lux = strtof(state->valuestring, NULL);
            int b = lux_to_brightness(lux);
            s_rest_last_lux = lux;
            s_rest_last_bright = b;
            if (s_hooks.apply_brightness) s_hooks.apply_brightness(b);  /* not persisted */
            ESP_LOGI(TAG, "HA REST: lux=%.1f -> brightness=%d", lux, b);
        }
    }
    cJSON_Delete(root);
}

static void ha_rest_task(void *arg)
{
    (void)arg;
    bool armed = false;   /* becomes true after the 5 s initial delay when enabled */
    for (;;) {
        if (!app_config_get_use_ha_rest()) {
            armed = false;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        if (!armed) { vTaskDelay(pdMS_TO_TICKS(5000)); armed = true; }
        if (wifi_up()) rest_poll_once();

        /* Sleep haPollInterval, in 1 s slices so a disable takes effect promptly. */
        uint32_t remain = app_config_get_ha_poll_int();
        if (remain < 1) remain = 1;
        for (uint32_t i = 0; i < remain && app_config_get_use_ha_rest(); i++)
            vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t mqtt_ha_rest_start(void)
{
    static TaskHandle_t rest_task = NULL;
    if (rest_task) return ESP_OK;
    if (xTaskCreate(ha_rest_task, "ha_rest", 8192, NULL, 4, &rest_task) != pdPASS) {
        ESP_LOGE(TAG, "HA REST task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
