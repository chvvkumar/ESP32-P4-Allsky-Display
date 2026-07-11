#include "app_config.h"

#include <string.h>
#include <math.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

static const char *TAG = "app_config";

/* Dedicated configured marker key. Presence in NVS means "a configuration has
 * been persisted". Independent of WiFi provisioning, unlike the legacy design
 * that keyed off wifi_ssid. */
#define CFG_MARKER_KEY "cfg_marker"

static allsky_config_t   s_cfg;
static SemaphoreHandle_t s_mtx;
static uint32_t          s_dirty;

#ifndef ALLSKY_FW_COMMIT
#define ALLSKY_FW_COMMIT "unknown"
#endif

/* ---- Locking ------------------------------------------------------------ */

static bool cfg_lock(void)
{
    if (!s_mtx) {
        return false;
    }
    if (xSemaphoreTakeRecursive(s_mtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
        return true;
    }
    ESP_LOGW(TAG, "config mutex timeout; proceeding unprotected");
    return false;
}

static void cfg_unlock(bool held)
{
    if (held) {
        xSemaphoreGiveRecursive(s_mtx);
    }
}

/* ---- Defaults ----------------------------------------------------------- */

static void source_slot_reset(image_source_t *s, uint32_t duration)
{
    s->url[0] = '\0';
    s->enabled = true;
    s->duration = duration;
    s->transform.scale_x = 1.2f;
    s->transform.scale_y = 1.2f;
    s->transform.offset_x = 0;
    s->transform.offset_y = 0;
    s->transform.rotation = 0.0f;
}

static void apply_defaults(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
#define D_STR(f, sz, k, j, g, d)          strlcpy(s_cfg.f, (d), sizeof(s_cfg.f));
#define D_SEC(f, sz, k, j, g, d)          strlcpy(s_cfg.f, (d), sizeof(s_cfg.f));
#define D_BOOL(f, k, j, g, d)             s_cfg.f = (d);
#define D_I32(f, k, j, g, d)              s_cfg.f = (d);
#define D_I32C(f, k, j, g, d, mn, mx)     s_cfg.f = (d);
#define D_U32(f, k, j, g, d)              s_cfg.f = (d);
#define D_U32C(f, k, j, g, d, mn, mx)     s_cfg.f = (d);
#define D_U8(f, k, j, g, d)               s_cfg.f = (d);
#define D_U8C(f, k, j, g, d, mn, mx)      s_cfg.f = (d);
#define D_F32(f, k, j, g, d)              s_cfg.f = (d);
#define D_F32C(f, k, j, g, d, mn, mx)     s_cfg.f = (d);
#define D_I32M(f, k, j, g, d)             s_cfg.f = (d);
    CONFIG_TABLE(D_STR, D_SEC, D_BOOL, D_I32, D_I32C, D_U32, D_U32C, D_U8, D_U8C,
                 D_F32, D_F32C, D_I32M)
#undef D_STR
#undef D_SEC
#undef D_BOOL
#undef D_I32
#undef D_I32C
#undef D_U32
#undef D_U32C
#undef D_U8
#undef D_U8C
#undef D_F32
#undef D_F32C
#undef D_I32M
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        source_slot_reset(&s_cfg.sources[i], s_cfg.def_img_dur);
    }
}

/* ---- Clamp / validate (load-time) --------------------------------------- */

static void clamp_transform(image_transform_t *t)
{
    if (t->scale_x < 0.1f) t->scale_x = 0.1f; else if (t->scale_x > 2.0f) t->scale_x = 2.0f;
    if (t->scale_y < 0.1f) t->scale_y = 0.1f; else if (t->scale_y > 2.0f) t->scale_y = 2.0f;
    float r = fmodf(t->rotation, 360.0f);
    if (r < 0.0f) r += 360.0f;
    int q = ((int)((r + 45.0f) / 90.0f)) % 4;
    t->rotation = (float)(q * 90);
}

static void validate_locked(void)
{
#define V_STR(f, sz, k, j, g, d)          s_cfg.f[sizeof(s_cfg.f) - 1] = '\0';
#define V_SEC(f, sz, k, j, g, d)          s_cfg.f[sizeof(s_cfg.f) - 1] = '\0';
#define V_BOOL(f, k, j, g, d)
#define V_I32(f, k, j, g, d)
#define V_I32C(f, k, j, g, d, mn, mx)     if (s_cfg.f < (mn)) s_cfg.f = (mn); else if (s_cfg.f > (mx)) s_cfg.f = (mx);
#define V_U32(f, k, j, g, d)
#define V_U32C(f, k, j, g, d, mn, mx)     if ((long)s_cfg.f < (long)(mn)) s_cfg.f = (mn); else if ((long)s_cfg.f > (long)(mx)) s_cfg.f = (mx);
#define V_U8(f, k, j, g, d)
#define V_U8C(f, k, j, g, d, mn, mx)      if (s_cfg.f < (mn)) s_cfg.f = (mn); else if (s_cfg.f > (mx)) s_cfg.f = (mx);
#define V_F32(f, k, j, g, d)
#define V_F32C(f, k, j, g, d, mn, mx)     if (s_cfg.f < (mn)) s_cfg.f = (mn); else if (s_cfg.f > (mx)) s_cfg.f = (mx);
#define V_I32M(f, k, j, g, d)
    CONFIG_TABLE(V_STR, V_SEC, V_BOOL, V_I32, V_I32C, V_U32, V_U32C, V_U8, V_U8C,
                 V_F32, V_F32C, V_I32M)
#undef V_STR
#undef V_SEC
#undef V_BOOL
#undef V_I32
#undef V_I32C
#undef V_U32
#undef V_U32C
#undef V_U8
#undef V_U8C
#undef V_F32
#undef V_F32C
#undef V_I32M
    if (s_cfg.disp_type != 1 && s_cfg.disp_type != 2) s_cfg.disp_type = 1;
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        s_cfg.sources[i].url[sizeof(s_cfg.sources[i].url) - 1] = '\0';
        clamp_transform(&s_cfg.sources[i].transform);
    }
    if (s_cfg.curr_img_idx < 0 || s_cfg.curr_img_idx >= s_cfg.img_src_cnt) {
        s_cfg.curr_img_idx = 0;
    }
}

/* ---- Generated typed getters / setters ---------------------------------- */

#define G_NUM_GET(f, T)  T app_config_get_##f(void) { bool h = cfg_lock(); T v = s_cfg.f; cfg_unlock(h); return v; }
#define G_NUM_SET(f, T, g) bool app_config_set_##f(T v) { bool h = cfg_lock(); bool ch = (s_cfg.f != v); if (ch) { s_cfg.f = v; s_dirty |= (g); } cfg_unlock(h); return ch; }
#define G_NUM_SETC(f, T, g, mn, mx) bool app_config_set_##f(T v) { if (v < (T)(mn)) v = (T)(mn); else if (v > (T)(mx)) v = (T)(mx); bool h = cfg_lock(); bool ch = (s_cfg.f != v); if (ch) { s_cfg.f = v; s_dirty |= (g); } cfg_unlock(h); return ch; }

#define S_STR(f, sz, k, j, g, d) \
    void app_config_get_##f(char *out, size_t len) { bool h = cfg_lock(); if (out && len) strlcpy(out, s_cfg.f, len); cfg_unlock(h); } \
    bool app_config_set_##f(const char *v) { if (!v) v = ""; bool h = cfg_lock(); bool ch = (strcmp(s_cfg.f, v) != 0); if (ch) { strlcpy(s_cfg.f, v, sizeof(s_cfg.f)); s_dirty |= (g); } cfg_unlock(h); return ch; }
#define S_SEC(f, sz, k, j, g, d)          S_STR(f, sz, k, j, g, d)
#define S_BOOL(f, k, j, g, d)             G_NUM_GET(f, bool) G_NUM_SET(f, bool, g)
#define S_I32(f, k, j, g, d)              G_NUM_GET(f, int32_t) G_NUM_SET(f, int32_t, g)
#define S_I32C(f, k, j, g, d, mn, mx)     G_NUM_GET(f, int32_t) G_NUM_SETC(f, int32_t, g, mn, mx)
#define S_U32(f, k, j, g, d)              G_NUM_GET(f, uint32_t) G_NUM_SET(f, uint32_t, g)
#define S_U32C(f, k, j, g, d, mn, mx)     G_NUM_GET(f, uint32_t) G_NUM_SETC(f, uint32_t, g, mn, mx)
#define S_U8(f, k, j, g, d)               G_NUM_GET(f, uint8_t) G_NUM_SET(f, uint8_t, g)
#define S_U8C(f, k, j, g, d, mn, mx)      G_NUM_GET(f, uint8_t) G_NUM_SETC(f, uint8_t, g, mn, mx)
#define S_F32(f, k, j, g, d)              G_NUM_GET(f, float) G_NUM_SET(f, float, g)
#define S_F32C(f, k, j, g, d, mn, mx)     G_NUM_GET(f, float) G_NUM_SETC(f, float, g, mn, mx)
/* Manual-setter fields: generated getter only. */
#define S_I32M(f, k, j, g, d)             G_NUM_GET(f, int32_t)
CONFIG_TABLE(S_STR, S_SEC, S_BOOL, S_I32, S_I32C, S_U32, S_U32C, S_U8, S_U8C,
             S_F32, S_F32C, S_I32M)
#undef S_STR
#undef S_SEC
#undef S_BOOL
#undef S_I32
#undef S_I32C
#undef S_U32
#undef S_U32C
#undef S_U8
#undef S_U8C
#undef S_F32
#undef S_F32C
#undef S_I32M
#undef G_NUM_GET
#undef G_NUM_SET
#undef G_NUM_SETC

/* Manual validated setters. */
bool app_config_set_disp_type(int32_t v)
{
    if (v != 1 && v != 2) {
        return false;
    }
    bool h = cfg_lock();
    bool ch = (s_cfg.disp_type != v);
    if (ch) { s_cfg.disp_type = v; s_dirty |= CFG_GRP_DISPLAY; }
    cfg_unlock(h);
    return ch;
}

bool app_config_set_curr_img_idx(int32_t v)
{
    bool h = cfg_lock();
    bool ch = false;
    if (v >= 0 && v < s_cfg.img_src_cnt) {
        ch = (s_cfg.curr_img_idx != v);
        if (ch) { s_cfg.curr_img_idx = v; s_dirty |= CFG_GRP_CYCLING; }
    }
    cfg_unlock(h);
    return ch;
}

/* ---- NVS load / save ---------------------------------------------------- */

static void load_scalars(nvs_handle_t h)
{
#define L_STR(f, sz, k, j, g, d)          do { size_t _l = sizeof(s_cfg.f); nvs_get_str(h, k, s_cfg.f, &_l); } while (0);
#define L_SEC(f, sz, k, j, g, d)          L_STR(f, sz, k, j, g, d)
#define L_BOOL(f, k, j, g, d)             do { uint8_t _v; if (nvs_get_u8(h, k, &_v) == ESP_OK) s_cfg.f = _v ? true : false; } while (0);
#define L_I32(f, k, j, g, d)              do { int32_t _v; if (nvs_get_i32(h, k, &_v) == ESP_OK) s_cfg.f = _v; } while (0);
#define L_I32C(f, k, j, g, d, mn, mx)     L_I32(f, k, j, g, d)
#define L_U32(f, k, j, g, d)              do { uint32_t _v; if (nvs_get_u32(h, k, &_v) == ESP_OK) s_cfg.f = _v; } while (0);
#define L_U32C(f, k, j, g, d, mn, mx)     L_U32(f, k, j, g, d)
#define L_U8(f, k, j, g, d)               do { uint8_t _v; if (nvs_get_u8(h, k, &_v) == ESP_OK) s_cfg.f = _v; } while (0);
#define L_U8C(f, k, j, g, d, mn, mx)      L_U8(f, k, j, g, d)
#define L_F32(f, k, j, g, d)              do { float _v; size_t _l = sizeof(_v); if (nvs_get_blob(h, k, &_v, &_l) == ESP_OK && _l == sizeof(_v)) s_cfg.f = _v; } while (0);
#define L_F32C(f, k, j, g, d, mn, mx)     L_F32(f, k, j, g, d)
#define L_I32M(f, k, j, g, d)             L_I32(f, k, j, g, d)
    CONFIG_TABLE(L_STR, L_SEC, L_BOOL, L_I32, L_I32C, L_U32, L_U32C, L_U8, L_U8C,
                 L_F32, L_F32C, L_I32M)
#undef L_STR
#undef L_SEC
#undef L_BOOL
#undef L_I32
#undef L_I32C
#undef L_U32
#undef L_U32C
#undef L_U8
#undef L_U8C
#undef L_F32
#undef L_F32C
#undef L_I32M
}

static void load_sources(nvs_handle_t h)
{
    char key[16];
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        image_source_t *s = &s_cfg.sources[i];
        size_t l = sizeof(s->url);
        snprintf(key, sizeof(key), "img_src_%d", i);
        nvs_get_str(h, key, s->url, &l);
        uint8_t en;
        snprintf(key, sizeof(key), "img_en_%d", i);
        if (nvs_get_u8(h, key, &en) == ESP_OK) s->enabled = en ? true : false;
        uint32_t dur;
        snprintf(key, sizeof(key), "img_dur_%d", i);
        if (nvs_get_u32(h, key, &dur) == ESP_OK) s->duration = dur;
        float fv;
        size_t fl;
        snprintf(key, sizeof(key), "img_tf_%d_sx", i);
        fl = sizeof(fv); if (nvs_get_blob(h, key, &fv, &fl) == ESP_OK && fl == sizeof(fv)) s->transform.scale_x = fv;
        snprintf(key, sizeof(key), "img_tf_%d_sy", i);
        fl = sizeof(fv); if (nvs_get_blob(h, key, &fv, &fl) == ESP_OK && fl == sizeof(fv)) s->transform.scale_y = fv;
        int32_t iv;
        snprintf(key, sizeof(key), "img_tf_%d_ox", i);
        if (nvs_get_i32(h, key, &iv) == ESP_OK) s->transform.offset_x = iv;
        snprintf(key, sizeof(key), "img_tf_%d_oy", i);
        if (nvs_get_i32(h, key, &iv) == ESP_OK) s->transform.offset_y = iv;
        snprintf(key, sizeof(key), "img_tf_%d_rot", i);
        fl = sizeof(fv); if (nvs_get_blob(h, key, &fv, &fl) == ESP_OK && fl == sizeof(fv)) s->transform.rotation = fv;
    }
}

static void save_scalars(nvs_handle_t h, uint32_t groups)
{
#define W_STR(f, sz, k, j, g, d)          if (groups & (g)) nvs_set_str(h, k, s_cfg.f);
#define W_SEC(f, sz, k, j, g, d)          W_STR(f, sz, k, j, g, d)
#define W_BOOL(f, k, j, g, d)             if (groups & (g)) nvs_set_u8(h, k, s_cfg.f ? 1 : 0);
#define W_I32(f, k, j, g, d)              if (groups & (g)) nvs_set_i32(h, k, s_cfg.f);
#define W_I32C(f, k, j, g, d, mn, mx)     W_I32(f, k, j, g, d)
#define W_U32(f, k, j, g, d)              if (groups & (g)) nvs_set_u32(h, k, s_cfg.f);
#define W_U32C(f, k, j, g, d, mn, mx)     W_U32(f, k, j, g, d)
#define W_U8(f, k, j, g, d)               if (groups & (g)) nvs_set_u8(h, k, s_cfg.f);
#define W_U8C(f, k, j, g, d, mn, mx)      W_U8(f, k, j, g, d)
#define W_F32(f, k, j, g, d)              if (groups & (g)) nvs_set_blob(h, k, &s_cfg.f, sizeof(s_cfg.f));
#define W_F32C(f, k, j, g, d, mn, mx)     W_F32(f, k, j, g, d)
#define W_I32M(f, k, j, g, d)             W_I32(f, k, j, g, d)
    CONFIG_TABLE(W_STR, W_SEC, W_BOOL, W_I32, W_I32C, W_U32, W_U32C, W_U8, W_U8C,
                 W_F32, W_F32C, W_I32M)
#undef W_STR
#undef W_SEC
#undef W_BOOL
#undef W_I32
#undef W_I32C
#undef W_U32
#undef W_U32C
#undef W_U8
#undef W_U8C
#undef W_F32
#undef W_F32C
#undef W_I32M
}

static void save_sources(nvs_handle_t h, uint32_t groups)
{
    char key[16];
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        const image_source_t *s = &s_cfg.sources[i];
        if (groups & CFG_GRP_IMAGE) {
            snprintf(key, sizeof(key), "img_src_%d", i);   nvs_set_str(h, key, s->url);
            snprintf(key, sizeof(key), "img_en_%d", i);    nvs_set_u8(h, key, s->enabled ? 1 : 0);
            snprintf(key, sizeof(key), "img_dur_%d", i);   nvs_set_u32(h, key, s->duration);
        }
        if (groups & CFG_GRP_TRANSFORMS) {
            snprintf(key, sizeof(key), "img_tf_%d_sx", i);  nvs_set_blob(h, key, &s->transform.scale_x, sizeof(float));
            snprintf(key, sizeof(key), "img_tf_%d_sy", i);  nvs_set_blob(h, key, &s->transform.scale_y, sizeof(float));
            snprintf(key, sizeof(key), "img_tf_%d_ox", i);  nvs_set_i32(h, key, s->transform.offset_x);
            snprintf(key, sizeof(key), "img_tf_%d_oy", i);  nvs_set_i32(h, key, s->transform.offset_y);
            snprintf(key, sizeof(key), "img_tf_%d_rot", i); nvs_set_blob(h, key, &s->transform.rotation, sizeof(float));
        }
    }
}

static esp_err_t save_groups(uint32_t groups)
{
    if (groups == 0) {
        ESP_LOGD(TAG, "save skipped: nothing dirty");
        return ESP_OK;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(ALLSKY_CFG_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    save_scalars(h, groups);
    save_sources(h, groups);
    nvs_set_u8(h, CFG_MARKER_KEY, 1);
    err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "saved groups 0x%04" PRIx32, groups);
    }
    return err;
}

/* ---- Lifecycle ---------------------------------------------------------- */

bool app_config_has_stored_config(void)
{
    nvs_handle_t h;
    if (nvs_open(ALLSKY_CFG_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }
    uint8_t marker = 0;
    esp_err_t err = nvs_get_u8(h, CFG_MARKER_KEY, &marker);
    nvs_close(h);
    return err == ESP_OK;
}

static void first_boot_provision(void)
{
    static const char *const defaults[] = {
        "https://i.imgur.com/EsstNmc.jpeg",
        "https://i.imgur.com/EtW1eaT.jpeg",
        "https://i.imgur.com/k23xBF5.jpeg",
        "https://i.imgur.com/BysRDbf.jpeg",
        "http://allskypi5.lan/current/resized/image.jpg",
    };
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        source_slot_reset(&s_cfg.sources[i], s_cfg.def_img_dur);
    }
    int n = (int)(sizeof(defaults) / sizeof(defaults[0]));
    for (int i = 0; i < n; ++i) {
        strlcpy(s_cfg.sources[i].url, defaults[i], sizeof(s_cfg.sources[i].url));
    }
    s_cfg.img_src_cnt = n;
    s_cfg.cycling_en = true;
    s_cfg.random_ord = false;
    s_cfg.cycle_intv = 30000;
    s_cfg.curr_img_idx = 0;
    s_dirty = CFG_GRP_IMAGE | CFG_GRP_CYCLING | CFG_GRP_TRANSFORMS;
    save_groups(s_dirty);
    s_dirty = 0;
    ESP_LOGI(TAG, "first-boot provisioning: %d default sources", n);
}

esp_err_t app_config_init(void)
{
    if (!s_mtx) {
        s_mtx = xSemaphoreCreateRecursiveMutex();
        if (!s_mtx) {
            return ESP_ERR_NO_MEM;
        }
    }
    apply_defaults();
    s_dirty = 0;

    if (app_config_has_stored_config()) {
        nvs_handle_t h;
        esp_err_t err = nvs_open(ALLSKY_CFG_NAMESPACE, NVS_READONLY, &h);
        if (err == ESP_OK) {
            load_scalars(h);
            load_sources(h);
            nvs_close(h);
        } else {
            ESP_LOGW(TAG, "stored config present but open failed: %s", esp_err_to_name(err));
        }
        validate_locked();
        s_dirty = 0;
        ESP_LOGI(TAG, "config loaded (%d sources)", s_cfg.img_src_cnt);
    } else {
        first_boot_provision();
    }
    return ESP_OK;
}

esp_err_t app_config_save(void)
{
    bool h = cfg_lock();
    uint32_t groups = s_dirty;
    esp_err_t err = save_groups(groups);
    if (err == ESP_OK) {
        s_dirty = 0;
    }
    cfg_unlock(h);
    return err;
}

uint32_t app_config_dirty_groups(void)
{
    bool h = cfg_lock();
    uint32_t d = s_dirty;
    cfg_unlock(h);
    return d;
}

void app_config_mark_dirty(uint32_t groups)
{
    bool h = cfg_lock();
    s_dirty |= groups;
    cfg_unlock(h);
}

esp_err_t app_config_factory_reset(void)
{
    bool h = cfg_lock();
    nvs_handle_t nh;
    if (nvs_open(ALLSKY_CFG_NAMESPACE, NVS_READWRITE, &nh) == ESP_OK) {
        nvs_erase_all(nh);
        nvs_commit(nh);
        nvs_close(nh);
    }
    apply_defaults();
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        source_slot_reset(&s_cfg.sources[i], s_cfg.def_img_dur);
    }
    s_cfg.img_src_cnt = 1;   /* keep at least one valid slot after reset */
    s_cfg.wifi_prov = false;
    s_cfg.wifi_ssid[0] = '\0';
    s_cfg.wifi_pwd[0] = '\0';
    s_dirty = CFG_GRP_ALL;
    esp_err_t err = save_groups(CFG_GRP_ALL);
    s_dirty = 0;
    cfg_unlock(h);
    ESP_LOGW(TAG, "factory reset complete");
    return err;
}

void app_config_snapshot(allsky_config_t *out)
{
    if (!out) {
        return;
    }
    bool h = cfg_lock();
    memcpy(out, &s_cfg, sizeof(*out));
    cfg_unlock(h);
}

const char *app_config_firmware_id(void)
{
    return ALLSKY_FW_COMMIT;
}

/* ---- Image source list -------------------------------------------------- */

int app_config_source_count(void)
{
    bool h = cfg_lock();
    int c = s_cfg.img_src_cnt;
    cfg_unlock(h);
    if (c < 1) c = 1;
    if (c > ALLSKY_MAX_SOURCES) c = ALLSKY_MAX_SOURCES;
    return c;
}

bool app_config_get_source(int index, image_source_t *out)
{
    if (!out || index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return false;
    }
    bool h = cfg_lock();
    *out = s_cfg.sources[index];
    cfg_unlock(h);
    return true;
}

void app_config_get_source_url(int index, char *out, size_t len)
{
    if (!out || !len) {
        return;
    }
    if (index < 0 || index >= s_cfg.img_src_cnt) {
        ESP_LOGW(TAG, "source url index %d out of range", index);
        out[0] = '\0';
        return;
    }
    bool h = cfg_lock();
    strlcpy(out, s_cfg.sources[index].url, len);
    cfg_unlock(h);
}

bool app_config_get_source_enabled(int index)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return true;
    }
    bool h = cfg_lock();
    bool v = s_cfg.sources[index].enabled;
    cfg_unlock(h);
    return v;
}

uint32_t app_config_get_source_duration(int index)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return 30;
    }
    bool h = cfg_lock();
    uint32_t v = s_cfg.sources[index].duration;
    cfg_unlock(h);
    return v;
}

void app_config_current_image_url(char *out, size_t len)
{
    if (!out || !len) {
        return;
    }
    bool h = cfg_lock();
    if (s_cfg.cycling_en && s_cfg.img_src_cnt > 0) {
        int idx = s_cfg.curr_img_idx;
        if (idx >= 0 && idx < s_cfg.img_src_cnt && s_cfg.sources[idx].url[0] != '\0') {
            strlcpy(out, s_cfg.sources[idx].url, len);
        } else {
            strlcpy(out, s_cfg.image_url, len);
        }
    } else {
        strlcpy(out, s_cfg.image_url, len);
    }
    cfg_unlock(h);
}

int app_config_add_source(const char *url)
{
    bool h = cfg_lock();
    int idx = -1;
    if (s_cfg.img_src_cnt < ALLSKY_MAX_SOURCES) {
        idx = s_cfg.img_src_cnt;
        image_source_t *s = &s_cfg.sources[idx];
        strlcpy(s->url, url ? url : "", sizeof(s->url));
        s->enabled = true;
        s->duration = s_cfg.def_img_dur;
        s->transform.scale_x = s_cfg.def_scale_x;
        s->transform.scale_y = s_cfg.def_scale_y;
        s->transform.offset_x = s_cfg.def_off_x;
        s->transform.offset_y = s_cfg.def_off_y;
        s->transform.rotation = s_cfg.def_rot;
        clamp_transform(&s->transform);
        s_cfg.img_src_cnt++;
        s_dirty |= CFG_GRP_IMAGE | CFG_GRP_TRANSFORMS;
    }
    cfg_unlock(h);
    return idx;
}

bool app_config_remove_source(int index)
{
    bool h = cfg_lock();
    bool ok = false;
    if (index >= 0 && index < s_cfg.img_src_cnt && s_cfg.img_src_cnt > 1) {
        for (int i = index; i < s_cfg.img_src_cnt - 1; ++i) {
            s_cfg.sources[i] = s_cfg.sources[i + 1];   /* url, transform, enabled AND duration all shift */
        }
        source_slot_reset(&s_cfg.sources[s_cfg.img_src_cnt - 1], s_cfg.def_img_dur);
        s_cfg.img_src_cnt--;
        if (s_cfg.curr_img_idx >= s_cfg.img_src_cnt) {
            s_cfg.curr_img_idx = 0;
        }
        s_dirty |= CFG_GRP_IMAGE | CFG_GRP_TRANSFORMS | CFG_GRP_CYCLING;
        ok = true;
    }
    cfg_unlock(h);
    return ok;
}

void app_config_clear_sources(void)
{
    bool h = cfg_lock();
    for (int i = 0; i < ALLSKY_MAX_SOURCES; ++i) {
        s_cfg.sources[i].url[0] = '\0';
        s_cfg.sources[i].enabled = true;
    }
    s_cfg.img_src_cnt = 0;
    s_cfg.curr_img_idx = 0;
    s_dirty |= CFG_GRP_IMAGE | CFG_GRP_CYCLING;
    cfg_unlock(h);
}

bool app_config_set_source_url(int index, const char *url)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return false;
    }
    bool h = cfg_lock();
    strlcpy(s_cfg.sources[index].url, url ? url : "", sizeof(s_cfg.sources[index].url));
    s_dirty |= CFG_GRP_IMAGE;
    cfg_unlock(h);
    return true;
}

bool app_config_set_source_enabled(int index, bool enabled)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return false;
    }
    bool h = cfg_lock();
    s_cfg.sources[index].enabled = enabled;
    s_dirty |= CFG_GRP_IMAGE;
    cfg_unlock(h);
    return true;
}

bool app_config_set_source_duration(int index, uint32_t duration_s)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES) {
        return false;
    }
    bool h = cfg_lock();
    s_cfg.sources[index].duration = duration_s;
    s_dirty |= CFG_GRP_IMAGE;
    cfg_unlock(h);
    return true;
}

bool app_config_set_source_transform(int index, const image_transform_t *tf)
{
    if (index < 0 || index >= ALLSKY_MAX_SOURCES || !tf) {
        return false;
    }
    bool h = cfg_lock();
    s_cfg.sources[index].transform = *tf;
    clamp_transform(&s_cfg.sources[index].transform);
    s_dirty |= CFG_GRP_TRANSFORMS;
    cfg_unlock(h);
    return true;
}

int app_config_build_shuffle_order(int *out, int max)
{
    if (!out || max <= 0) {
        return 0;
    }
    bool h = cfg_lock();
    int n = s_cfg.img_src_cnt;
    cfg_unlock(h);
    if (n > max) n = max;
    if (n > ALLSKY_MAX_SOURCES) n = ALLSKY_MAX_SOURCES;
    for (int i = 0; i < n; ++i) {
        out[i] = i;
    }
    for (int i = n - 1; i > 0; --i) {
        uint32_t r = esp_random() % (uint32_t)(i + 1);
        int tmp = out[i];
        out[i] = out[r];
        out[r] = tmp;
    }
    return n;
}

/* ---- Backup serialize / restore-apply (table-driven) -------------------- */

void app_config_backup_serialize(void *cfg_obj, bool include_secrets)
{
    cJSON *o = (cJSON *)cfg_obj;
    if (!o) {
        return;
    }
    bool h = cfg_lock();
#define SER_STR(f, sz, k, j, g, d)         if (j) cJSON_AddStringToObject(o, (j), s_cfg.f);
#define SER_SEC(f, sz, k, j, g, d)         if ((j) && include_secrets) cJSON_AddStringToObject(o, (j), s_cfg.f);
#define SER_BOOL(f, k, j, g, d)            if (j) cJSON_AddBoolToObject(o, (j), s_cfg.f);
#define SER_I32(f, k, j, g, d)             if (j) cJSON_AddNumberToObject(o, (j), (double)s_cfg.f);
#define SER_I32C(f, k, j, g, d, mn, mx)    SER_I32(f, k, j, g, d)
#define SER_U32(f, k, j, g, d)             if (j) cJSON_AddNumberToObject(o, (j), (double)s_cfg.f);
#define SER_U32C(f, k, j, g, d, mn, mx)    SER_U32(f, k, j, g, d)
#define SER_U8(f, k, j, g, d)              if (j) cJSON_AddNumberToObject(o, (j), (double)s_cfg.f);
#define SER_U8C(f, k, j, g, d, mn, mx)     SER_U8(f, k, j, g, d)
#define SER_F32(f, k, j, g, d)             if (j) cJSON_AddNumberToObject(o, (j), (double)s_cfg.f);
#define SER_F32C(f, k, j, g, d, mn, mx)    SER_F32(f, k, j, g, d)
#define SER_I32M(f, k, j, g, d)            if (j) cJSON_AddNumberToObject(o, (j), (double)s_cfg.f);
    CONFIG_TABLE(SER_STR, SER_SEC, SER_BOOL, SER_I32, SER_I32C, SER_U32, SER_U32C,
                 SER_U8, SER_U8C, SER_F32, SER_F32C, SER_I32M)
#undef SER_STR
#undef SER_SEC
#undef SER_BOOL
#undef SER_I32
#undef SER_I32C
#undef SER_U32
#undef SER_U32C
#undef SER_U8
#undef SER_U8C
#undef SER_F32
#undef SER_F32C
#undef SER_I32M
    /* imageSources: exactly img_src_cnt configured entries. */
    cJSON *arr = cJSON_AddArrayToObject(o, "imageSources");
    if (arr) {
        int cnt = s_cfg.img_src_cnt;
        if (cnt < 0) cnt = 0;
        if (cnt > ALLSKY_MAX_SOURCES) cnt = ALLSKY_MAX_SOURCES;
        for (int i = 0; i < cnt; ++i) {
            const image_source_t *s = &s_cfg.sources[i];
            cJSON *e = cJSON_CreateObject();
            if (!e) continue;
            cJSON_AddStringToObject(e, "url", s->url);
            cJSON_AddBoolToObject(e, "enabled", s->enabled);
            cJSON_AddNumberToObject(e, "duration", (double)s->duration);
            cJSON_AddNumberToObject(e, "scaleX", (double)s->transform.scale_x);
            cJSON_AddNumberToObject(e, "scaleY", (double)s->transform.scale_y);
            cJSON_AddNumberToObject(e, "offsetX", (double)s->transform.offset_x);
            cJSON_AddNumberToObject(e, "offsetY", (double)s->transform.offset_y);
            cJSON_AddNumberToObject(e, "rotation", (double)s->transform.rotation);
            cJSON_AddItemToArray(arr, e);
        }
    }
    cfg_unlock(h);
}

int app_config_restore_apply_scalars(const void *cfg_obj)
{
    const cJSON *o = (const cJSON *)cfg_obj;
    if (!o) {
        return 0;
    }
    int applied = 0;
#define RA_STR(f, sz, k, j, g, d)          do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsString(_it)) { app_config_set_##f(_it->valuestring); applied++; } } } while (0);
#define RA_SEC(f, sz, k, j, g, d)          do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsString(_it) && _it->valuestring[0] != '\0') { app_config_set_##f(_it->valuestring); applied++; } } } while (0);
#define RA_BOOL(f, k, j, g, d)             do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsBool(_it)) { app_config_set_##f(cJSON_IsTrue(_it)); applied++; } } } while (0);
#define RA_I32(f, k, j, g, d)              do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsNumber(_it)) { app_config_set_##f((int32_t)_it->valuedouble); applied++; } } } while (0);
#define RA_I32C(f, k, j, g, d, mn, mx)     RA_I32(f, k, j, g, d)
#define RA_U32(f, k, j, g, d)              do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsNumber(_it)) { app_config_set_##f((uint32_t)_it->valuedouble); applied++; } } } while (0);
#define RA_U32C(f, k, j, g, d, mn, mx)     RA_U32(f, k, j, g, d)
#define RA_U8(f, k, j, g, d)               do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsNumber(_it)) { app_config_set_##f((uint8_t)_it->valueint); applied++; } } } while (0);
#define RA_U8C(f, k, j, g, d, mn, mx)      RA_U8(f, k, j, g, d)
#define RA_F32(f, k, j, g, d)              do { const char *_k = (j); if (_k) { cJSON *_it = cJSON_GetObjectItem(o, _k); if (cJSON_IsNumber(_it)) { app_config_set_##f((float)_it->valuedouble); applied++; } } } while (0);
#define RA_F32C(f, k, j, g, d, mn, mx)     RA_F32(f, k, j, g, d)
/* displayType and currentImageIndex are sequenced by the restore engine. */
#define RA_I32M(f, k, j, g, d)
    CONFIG_TABLE(RA_STR, RA_SEC, RA_BOOL, RA_I32, RA_I32C, RA_U32, RA_U32C, RA_U8,
                 RA_U8C, RA_F32, RA_F32C, RA_I32M)
#undef RA_STR
#undef RA_SEC
#undef RA_BOOL
#undef RA_I32
#undef RA_I32C
#undef RA_U32
#undef RA_U32C
#undef RA_U8
#undef RA_U8C
#undef RA_F32
#undef RA_F32C
#undef RA_I32M
    return applied;
}

bool app_config_is_known_backup_key(const char *key)
{
    if (!key) {
        return false;
    }
    if (strcmp(key, "imageSources") == 0) {
        return true;
    }
#define K_ROW(j)                          do { const char *_k = (j); if (_k && strcmp(key, _k) == 0) return true; } while (0);
#define K_STR(f, sz, k, j, g, d)          K_ROW(j)
#define K_SEC(f, sz, k, j, g, d)          K_ROW(j)
#define K_BOOL(f, k, j, g, d)             K_ROW(j)
#define K_I32(f, k, j, g, d)              K_ROW(j)
#define K_I32C(f, k, j, g, d, mn, mx)     K_ROW(j)
#define K_U32(f, k, j, g, d)              K_ROW(j)
#define K_U32C(f, k, j, g, d, mn, mx)     K_ROW(j)
#define K_U8(f, k, j, g, d)               K_ROW(j)
#define K_U8C(f, k, j, g, d, mn, mx)      K_ROW(j)
#define K_F32(f, k, j, g, d)              K_ROW(j)
#define K_F32C(f, k, j, g, d, mn, mx)     K_ROW(j)
#define K_I32M(f, k, j, g, d)             K_ROW(j)
    CONFIG_TABLE(K_STR, K_SEC, K_BOOL, K_I32, K_I32C, K_U32, K_U32C, K_U8, K_U8C,
                 K_F32, K_F32C, K_I32M)
#undef K_STR
#undef K_SEC
#undef K_BOOL
#undef K_I32
#undef K_I32C
#undef K_U32
#undef K_U32C
#undef K_U8
#undef K_U8C
#undef K_F32
#undef K_F32C
#undef K_I32M
#undef K_ROW
    return false;
}
