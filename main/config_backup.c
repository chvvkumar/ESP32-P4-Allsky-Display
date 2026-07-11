#include "config_backup.h"
#include "app_config.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "config_backup";

/* ---- Export ------------------------------------------------------------- */

esp_err_t config_backup_export(char **out_json, bool include_secrets)
{
    if (!out_json) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_json = NULL;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }

    char devname[64];
    app_config_get_device_name(devname, sizeof(devname));

    cJSON *meta = cJSON_AddObjectToObject(root, "_meta");
    if (meta) {
        cJSON_AddNumberToObject(meta, "schemaVersion", ALLSKY_SCHEMA_VERSION);
        cJSON_AddStringToObject(meta, "firmware", app_config_firmware_id());
        cJSON_AddStringToObject(meta, "deviceName", devname);
        cJSON_AddNumberToObject(meta, "displayType", (double)app_config_get_disp_type());
        cJSON_AddBoolToObject(meta, "secretsIncluded", include_secrets);
    }

    cJSON *cfg = cJSON_AddObjectToObject(root, "config");
    if (!cfg) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    app_config_backup_serialize(cfg, include_secrets);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) {
        return ESP_ERR_NO_MEM;
    }
    *out_json = str;
    return ESP_OK;
}

/* ---- Restore ------------------------------------------------------------ */

/* Pre-apply migration hook keyed on the document's schema version. A no-op at
 * version 1; a future rename/unit change adds a case here that rewrites keys in
 * the parsed `config` object before the lenient apply runs. */
static void migrate_document(cJSON *cfg, int from_version)
{
    (void)cfg;
    (void)from_version;
    /* No migrations defined for schema version 1. */
}

static void apply_image_sources(const cJSON *arr, int *applied, int *skipped)
{
    app_config_clear_sources();
    int added = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, arr) {
        if (added >= ALLSKY_MAX_SOURCES) {
            (*skipped)++;
            continue;
        }
        const char *url = "";
        cJSON *u = cJSON_GetObjectItem(entry, "url");
        if (cJSON_IsString(u)) {
            url = u->valuestring;
        }
        int idx = app_config_add_source(url);
        if (idx < 0) {
            (*skipped)++;
            continue;
        }
        cJSON *en = cJSON_GetObjectItem(entry, "enabled");
        if (cJSON_IsBool(en)) {
            app_config_set_source_enabled(idx, cJSON_IsTrue(en));
        }
        cJSON *dur = cJSON_GetObjectItem(entry, "duration");
        if (cJSON_IsNumber(dur)) {
            app_config_set_source_duration(idx, (uint32_t)dur->valuedouble);
        }
        image_source_t cur;
        if (app_config_get_source(idx, &cur)) {
            image_transform_t tf = cur.transform;
            cJSON *it;
            if (cJSON_IsNumber((it = cJSON_GetObjectItem(entry, "scaleX"))))  tf.scale_x  = (float)it->valuedouble;
            if (cJSON_IsNumber((it = cJSON_GetObjectItem(entry, "scaleY"))))  tf.scale_y  = (float)it->valuedouble;
            if (cJSON_IsNumber((it = cJSON_GetObjectItem(entry, "offsetX")))) tf.offset_x = (int32_t)it->valuedouble;
            if (cJSON_IsNumber((it = cJSON_GetObjectItem(entry, "offsetY")))) tf.offset_y = (int32_t)it->valuedouble;
            if (cJSON_IsNumber((it = cJSON_GetObjectItem(entry, "rotation")))) tf.rotation = (float)it->valuedouble;
            app_config_set_source_transform(idx, &tf);
        }
        added++;
        (*applied)++;
    }
}

esp_err_t config_backup_restore(const char *json, size_t len, config_restore_result_t *res)
{
    if (!json || !res) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(res, 0, sizeof(*res));

    cJSON *root = (len > 0) ? cJSON_ParseWithLength(json, len) : cJSON_Parse(json);
    if (!root) {
        const char *ep = cJSON_GetErrorPtr();
        snprintf(res->message, sizeof(res->message), "JSON parse error: %s",
                 ep ? ep : "invalid document");
        res->ok = false;
        return ESP_OK;
    }

    cJSON *cfg = cJSON_GetObjectItem(root, "config");
    if (!cJSON_IsObject(cfg)) {
        strlcpy(res->message, "Missing or invalid 'config' object", sizeof(res->message));
        res->ok = false;
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON *meta = cJSON_GetObjectItem(root, "_meta");
    int file_version = 0;
    if (cJSON_IsObject(meta)) {
        cJSON *sv = cJSON_GetObjectItem(meta, "schemaVersion");
        if (cJSON_IsNumber(sv)) {
            file_version = sv->valueint;
        }
    }
    res->file_version = file_version;
    res->version_mismatch = (file_version != ALLSKY_SCHEMA_VERSION);

    if (file_version < ALLSKY_SCHEMA_VERSION) {
        migrate_document(cfg, file_version);
    }

    int applied = 0;
    int skipped = 0;

    /* Lenient scalar apply (secrets skipped when empty; setters clamp). */
    applied += app_config_restore_apply_scalars(cfg);

    /* displayType is validated (1 or 2) and applied like any scalar. */
    cJSON *dt = cJSON_GetObjectItem(cfg, "displayType");
    if (cJSON_IsNumber(dt)) {
        app_config_set_disp_type((int32_t)dt->valuedouble);
        applied++;
    }

    /* imageSources replaces the whole list. */
    cJSON *arr = cJSON_GetObjectItem(cfg, "imageSources");
    if (arr) {
        if (cJSON_IsArray(arr)) {
            apply_image_sources(arr, &applied, &skipped);
        } else {
            skipped++;   /* present but not an array */
        }
    }

    /* currentImageIndex applied only after the source list is rebuilt. */
    cJSON *cii = cJSON_GetObjectItem(cfg, "currentImageIndex");
    if (cJSON_IsNumber(cii)) {
        app_config_set_curr_img_idx((int32_t)cii->valuedouble);
        applied++;
    }

    /* Count unrecognized keys as skipped. */
    cJSON *child = NULL;
    cJSON_ArrayForEach(child, cfg) {
        if (!app_config_is_known_backup_key(child->string)) {
            skipped++;
        }
    }

    /* Force a full persist regardless of which groups the setters flagged. */
    app_config_mark_dirty(CFG_GRP_ALL);
    app_config_save();

    res->applied = applied;
    res->skipped = skipped;
    res->ok = true;
    if (res->version_mismatch) {
        snprintf(res->message, sizeof(res->message),
                 "Configuration restored (schema v%d applied to firmware v%d). Device restarting now...",
                 file_version, ALLSKY_SCHEMA_VERSION);
    } else {
        strlcpy(res->message, "Configuration restored. Device restarting now...",
                sizeof(res->message));
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "restore ok: applied=%d skipped=%d fileVersion=%d mismatch=%d",
             applied, skipped, file_version, res->version_mismatch);
    return ESP_OK;
}
