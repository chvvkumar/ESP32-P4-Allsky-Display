#include "web_internal.h"
#include "display_defs.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_timer.h"

static const char *TAG = "web_api_images";

/* ---- Editing / tuning pause state ---------------------------------------- */

#define EDIT_BACKSTOP_US (600LL * 1000000LL)  /* 10 minutes */

static bool    s_paused = false;
static int     s_tune_index = -1;     /* runtime-only tune index, -1 = not tuning */
static int64_t s_last_activity_us = 0;

void web_editing_touch(void) { s_last_activity_us = esp_timer_get_time(); }
bool web_editing_paused(void) { return s_paused; }
int  web_editing_tune_index(void) { return s_tune_index; }

static void editing_pause(void)
{
    s_paused = true;
    web_editing_touch();
    web_hook_image_set_editing_paused(true);
}

static void editing_resume(void)
{
    s_paused = false;
    s_tune_index = -1;
    web_hook_image_set_editing_paused(false);
}

void web_editing_tick(void)
{
    if (s_paused && (esp_timer_get_time() - s_last_activity_us) > EDIT_BACKSTOP_US) {
        ESP_LOGI(TAG, "edit inactivity backstop: resuming cycling");
        editing_resume();
    }
}

/* ---- Small form helpers -------------------------------------------------- */

static bool form_int(const char *body, const char *key, int *out)
{
    char v[32];
    if (!web_form_get(body, key, v, sizeof(v)) || v[0] == '\0') return false;
    *out = atoi(v);
    return true;
}

static bool form_float(const char *body, const char *key, float *out)
{
    char v[32];
    if (!web_form_get(body, key, v, sizeof(v)) || v[0] == '\0') return false;
    *out = strtof(v, NULL);
    return true;
}

static bool index_valid(int index) { return index >= 0 && index < app_config_source_count(); }

static bool source_is_moon(int index)
{
    char url[256];
    app_config_get_source_url(index, url, sizeof(url));
    return strncmp(url, "moon://", 7) == 0;
}

/* ---- Add / preset -------------------------------------------------------- */

esp_err_t web_add_source_handler(httpd_req_t *req)
{
    char body[600];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    char url[300];
    if (!web_form_get(body, "url", url, sizeof(url)) || url[0] == '\0')
        return web_send_400(req, "Missing required parameter: url");
    if (app_config_source_count() >= WEB_MAX_SOURCES)
        return web_send_400(req, "Maximum image sources reached");
    if (app_config_add_source(url) < 0)
        return web_send_400(req, "Maximum image sources reached");
    app_config_save();
    web_hook_mqtt_sources_changed();
    return web_send_ok(req, "Source added");
}

esp_err_t web_add_preset_handler(httpd_req_t *req)
{
    char body[300];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    char id[64];
    if (!web_form_get(body, "id", id, sizeof(id)) || id[0] == '\0')
        return web_send_400(req, "Missing required parameter: id");
    if (app_config_source_count() >= WEB_MAX_SOURCES)
        return web_send_400(req, "Maximum image sources reached");

    int panel_w = allsky_panel_profile().width;
    image_transform_t tf = { 1.0f, 1.0f, 0, 0, 0.0f };
    const char *url = NULL;

    if (strcmp(id, "__moon__") == 0) {
        url = "moon://default";
        float s = 0.8f;
        if (s < 0.1f) s = 0.1f;
        if (s > WEB_MAX_SCALE) s = WEB_MAX_SCALE;
        tf.scale_x = tf.scale_y = s;
    } else {
        const web_preset_t *p = NULL;
        for (int i = 0; i < g_web_preset_count; i++)
            if (strcmp(g_web_presets[i].id, id) == 0) { p = &g_web_presets[i]; break; }
        if (!p) return web_send_status(req, 404, "error", "Unknown preset id", NULL);
        url = p->url;
        float s = (float)panel_w / ((float)p->nominal_px * (float)p->crop_pct / 100.0f);
        if (s < 0.1f) s = 0.1f;
        if (s > WEB_MAX_SCALE) s = WEB_MAX_SCALE;
        tf.scale_x = tf.scale_y = s;
    }

    int idx = app_config_add_source(url);
    if (idx < 0) return web_send_400(req, "Maximum image sources reached");
    app_config_set_source_transform(idx, &tf);
    app_config_save();
    web_hook_mqtt_sources_changed();
    return web_send_ok(req, "Preset added");
}

/* ---- Remove / update / clear / bulk -------------------------------------- */

esp_err_t web_remove_source_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");
    if (app_config_source_count() <= 1) return web_send_400(req, "Cannot remove the last remaining source");
    if (!app_config_remove_source(index)) return web_send_400(req, "Invalid index");
    app_config_save();
    web_hook_mqtt_sources_changed();
    return web_send_ok(req, "Source removed");
}

esp_err_t web_update_source_handler(httpd_req_t *req)
{
    char body[600];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    char url[300];
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!web_form_get(body, "url", url, sizeof(url)) || url[0] == '\0')
        return web_send_400(req, "Missing required parameter: url");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");
    app_config_set_source_url(index, url);
    app_config_save();
    web_hook_mqtt_sources_changed();
    return web_send_ok(req, "Source updated");
}

esp_err_t web_clear_sources_handler(httpd_req_t *req)
{
    /* Storage clear -> count 0, then re-add the legacy single URL -> count 1. */
    char legacy[256];
    app_config_get_image_url(legacy, sizeof(legacy));
    app_config_clear_sources();
    if (legacy[0] == '\0') strcpy(legacy, "http://allskypi5.lan/current/resized/image.jpg");
    app_config_add_source(legacy);
    app_config_save();
    web_hook_mqtt_sources_changed();
    return web_send_ok(req, "Sources reset to single default");
}

esp_err_t web_bulk_delete_handler(httpd_req_t *req)
{
    char body[600];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    char indices[400];
    if (!web_form_get(body, "indices", indices, sizeof(indices)))
        return web_send_400(req, "Missing required parameter: indices");

    cJSON *arr = cJSON_Parse(indices);
    if (!cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return web_send_400(req, "Invalid indices format"); }
    int n = cJSON_GetArraySize(arr);
    if (n == 0) { cJSON_Delete(arr); return web_send_400(req, "Invalid indices format"); }

    int total = app_config_source_count();
    if (n >= total) { cJSON_Delete(arr); return web_send_400(req, "Cannot delete all sources. At least one must remain."); }

    /* Collect + sort descending so lower indices stay valid. */
    int list[WEB_MAX_SOURCES];
    int m = 0;
    for (int i = 0; i < n && m < WEB_MAX_SOURCES; i++) {
        cJSON *e = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsNumber(e)) list[m++] = e->valueint;
    }
    cJSON_Delete(arr);
    for (int i = 0; i < m; i++)
        for (int j = i + 1; j < m; j++)
            if (list[j] > list[i]) { int t = list[i]; list[i] = list[j]; list[j] = t; }

    int deleted = 0;
    for (int i = 0; i < m; i++) {
        if (app_config_source_count() <= 1) break;
        if (list[i] >= 0 && list[i] < app_config_source_count())
            if (app_config_remove_source(list[i])) deleted++;
    }
    if (deleted == 0) return web_send_status(req, 500, "error", "No sources deleted", NULL);
    app_config_save();
    web_hook_mqtt_sources_changed();

    char extra[96];
    snprintf(extra, sizeof(extra), "\"deleted\":%d,\"remaining\":%d", deleted, app_config_source_count());
    char msg[96];
    snprintf(msg, sizeof(msg), "Successfully deleted %d of %d source(s)", deleted, m);
    return web_send_status(req, 200, "success", msg, extra);
}

/* ---- Toggle / duration --------------------------------------------------- */

esp_err_t web_toggle_enabled_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");

    bool now = !app_config_get_source_enabled(index);
    app_config_set_source_enabled(index, now);
    app_config_save();

    int cur = web_hook_image_current_index();
    bool switched = false;
    if (!now && index == cur) {            /* disabled the displayed image */
        web_hook_image_next();
        switched = true;
    } else if (now && index != cur) {      /* enabled a different image: switch to it */
        image_source_t s;
        if (app_config_get_source(index, &s)) {
            app_config_set_curr_img_idx(index);
            app_config_save();
            web_hook_image_tune(index, true);
            switched = true;
        }
    }
    char extra[48];
    snprintf(extra, sizeof(extra), "\"enabled\":%s,\"switched\":%s",
             now ? "true" : "false", switched ? "true" : "false");
    return web_send_status(req, 200, "success", "", extra);
}

esp_err_t web_update_duration_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index, duration;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");
    if (!form_int(body, "duration", &duration)) return web_send_400(req, "Missing required parameter: duration");
    if (duration < 5 || duration > 3600) return web_send_400(req, "Duration must be between 5 and 3600 seconds");
    app_config_set_source_duration(index, (uint32_t)duration);
    app_config_save();
    return web_send_ok(req, "");
}

/* ---- Transforms ---------------------------------------------------------- */

static void apply_transform_live(int index)
{
    if (!s_paused) return;
    if (index != web_hook_image_current_index()) return;
    web_editing_touch();
    web_hook_image_rerender();
}

esp_err_t web_update_transform_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    char property[16], valstr[32];
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");
    if (!web_form_get(body, "property", property, sizeof(property)))
        return web_send_400(req, "Missing required parameter: property");
    if (!web_form_get(body, "value", valstr, sizeof(valstr)))
        return web_send_400(req, "Missing required parameter: value");

    image_source_t s;
    if (!app_config_get_source(index, &s)) return web_send_400(req, "Invalid index");
    image_transform_t tf = s.transform;
    float fv = strtof(valstr, NULL);

    if (strcmp(property, "scaleX") == 0)      tf.scale_x = fv;
    else if (strcmp(property, "scaleY") == 0) tf.scale_y = fv;
    else if (strcmp(property, "offsetX") == 0) tf.offset_x = (int)fv;
    else if (strcmp(property, "offsetY") == 0) tf.offset_y = (int)fv;
    else if (strcmp(property, "rotation") == 0) tf.rotation = fv;
    else return web_send_status(req, 200, "error", "Invalid property name", NULL); /* legacy: 200 */

    app_config_set_source_transform(index, &tf);
    app_config_save();
    apply_transform_live(index);
    return web_send_ok(req, "");
}

esp_err_t web_copy_defaults_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");

    image_transform_t tf = {
        .scale_x = app_config_get_def_scale_x(),
        .scale_y = app_config_get_def_scale_y(),
        .offset_x = app_config_get_def_off_x(),
        .offset_y = app_config_get_def_off_y(),
        .rotation = app_config_get_def_rot(),
    };
    app_config_set_source_transform(index, &tf);
    app_config_save();
    if (index == web_hook_image_current_index()) web_hook_image_rerender();
    return web_send_ok(req, "Reset to defaults");
}

esp_err_t web_apply_transform_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");

    editing_pause();
    if (index != (int)app_config_get_curr_img_idx()) {
        app_config_set_curr_img_idx(index);
        app_config_save();
        web_hook_image_tune(index, true);
    } else {
        web_hook_image_rerender();
    }
    return web_send_status(req, 200, "success", "Transform applied successfully", NULL);
}

/* ---- Select / tune ------------------------------------------------------- */

esp_err_t web_select_image_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");

    editing_pause();
    app_config_set_curr_img_idx(index);
    app_config_save();
    web_hook_image_tune(index, true);   /* persist + hold on screen */
    s_tune_index = index;

    char extra[32];
    snprintf(extra, sizeof(extra), "\"index\":%d", index);
    return web_send_status(req, 200, "success", "", extra);
}

esp_err_t web_tune_handler(httpd_req_t *req)
{
    char body[128];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");
    int index;
    if (!form_int(body, "index", &index)) return web_send_400(req, "Missing required parameter: index");
    if (!index_valid(index)) return web_send_400(req, "Invalid index");

    editing_pause();
    s_tune_index = index;
    web_hook_image_tune(index, false);  /* runtime-only, not persisted */
    return web_send_ok(req, "");
}

esp_err_t web_tune_stop_handler(httpd_req_t *req)
{
    editing_resume();
    web_hook_image_tune_stop();
    return web_send_ok(req, "");
}

esp_err_t web_clear_editing_handler(httpd_req_t *req)
{
    editing_resume();
    web_hook_image_tune_stop();
    return web_send_ok(req, "");
}

/* ---- Next / refresh ------------------------------------------------------ */

esp_err_t web_next_image_handler(httpd_req_t *req)
{
    web_hook_image_next();
    return web_send_status(req, 200, "queued", "Image download queued", NULL);
}

esp_err_t web_force_refresh_handler(httpd_req_t *req)
{
    web_hook_image_queue_download();
    return web_send_status(req, 200, "queued", "Image download queued", NULL);
}

/* ---- Moon ---------------------------------------------------------------- */

esp_err_t web_set_moon_handler(httpd_req_t *req)
{
    char body[512];
    if (web_read_body(req, body, sizeof(body)) < 0) return web_send_400(req, "Invalid request body");

    float f; int i;
    if (form_float(body, "lat", &f))   app_config_set_moonLat(f);
    if (form_float(body, "lon", &f))   app_config_set_moonLon(f);
    if (form_int(body, "bg", &i))      app_config_set_moonBg(i);
    if (form_int(body, "flipu", &i))   app_config_set_mFlipU((uint8_t)(i ? 1 : 0));
    if (form_int(body, "flipv", &i))   app_config_set_mFlipV((uint8_t)(i ? 1 : 0));
    if (form_float(body, "roll", &f))  app_config_set_mRollOff(f);
    if (form_float(body, "yaw", &f))   app_config_set_mYawOff(f);
    if (form_float(body, "pitch", &f)) app_config_set_mPitchOff(f);
    if (form_int(body, "northup", &i)) app_config_set_mNorthUp((uint8_t)(i ? 1 : 0));
    if (form_int(body, "light", &i))   app_config_set_mDragLM((uint8_t)i);
    if (form_int(body, "spin", &i))    app_config_set_mSpinMode((uint8_t)i);
    if (form_int(body, "spinret", &i)) app_config_set_mSpinRet((uint8_t)i);
    app_config_save();

    /* Live re-render if the displayed source is a moon source. */
    if (source_is_moon(web_hook_image_current_index())) web_hook_image_rerender();

    return web_send_status(req, 200, "success", "Moon settings saved", NULL);
}
