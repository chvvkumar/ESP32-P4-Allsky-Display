#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result of a restore attempt. `ok` is false for the two hard errors (JSON parse
 * failure, missing/invalid config object); the web layer maps those to HTTP 400
 * and does NOT reboot. On ok, the caller persists nothing further (restore
 * already saved) and schedules the reboot. */
typedef struct {
    bool ok;
    int  applied;
    int  skipped;
    int  file_version;
    bool version_mismatch;
    char message[192];
} config_restore_result_t;

/* Build the schema-version-1 backup document. On success sets *out_json to a
 * heap buffer (caller frees with free()) containing the serialized document.
 * include_secrets controls whether wifiPassword/mqttPassword/haAccessToken are
 * emitted and the _meta.secretsIncluded flag. */
esp_err_t config_backup_export(char **out_json, bool include_secrets);

/* Apply a backup document. `json` need not be NUL-terminated when `len` > 0;
 * pass len == 0 for a NUL-terminated string. Never returns an error for a
 * malformed document: the outcome is reported entirely through `res` (res->ok
 * distinguishes success from a 400-class failure). Returns ESP_ERR_INVALID_ARG
 * only for a NULL argument. On success the config has already been saved. */
esp_err_t config_backup_restore(const char *json, size_t len, config_restore_result_t *res);

#ifdef __cplusplus
}
#endif
