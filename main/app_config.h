#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "config_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Backup document schema version. Bump ONLY on a rename/removal/unit change and
 * add a migration in config_backup.c; additive keys never need a bump. */
#define ALLSKY_SCHEMA_VERSION 1

/* NVS namespace owned entirely by this subsystem. */
#define ALLSKY_CFG_NAMESPACE  "allsky_config"

/* Image source list bounds. */
#define ALLSKY_MAX_SOURCES    10

/* Per-image transform. */
typedef struct {
    float   scale_x;   /* clamp [0.1, 2.0] */
    float   scale_y;   /* clamp [0.1, 2.0] */
    int32_t offset_x;  /* pixels */
    int32_t offset_y;  /* pixels */
    float   rotation;  /* snapped to 0/90/180/270 */
} image_transform_t;

/* One image source slot. All 10 slots are always persisted. */
typedef struct {
    char              url[256];
    bool              enabled;
    uint32_t          duration;   /* seconds */
    image_transform_t transform;
} image_source_t;

/* In-RAM configuration. Scalar fields are generated from CONFIG_TABLE; the image
 * source array is hand-managed. Access it only through the accessors below so the
 * mutex and dirty tracking are honored. */
typedef struct {
#define CFG_F_STR(f, sz, k, j, g, d)          char     f[sz];
#define CFG_F_SEC(f, sz, k, j, g, d)          char     f[sz];
#define CFG_F_BOOL(f, k, j, g, d)             bool     f;
#define CFG_F_I32(f, k, j, g, d)              int32_t  f;
#define CFG_F_I32C(f, k, j, g, d, mn, mx)     int32_t  f;
#define CFG_F_U32(f, k, j, g, d)              uint32_t f;
#define CFG_F_U32C(f, k, j, g, d, mn, mx)     uint32_t f;
#define CFG_F_U8(f, k, j, g, d)               uint8_t  f;
#define CFG_F_U8C(f, k, j, g, d, mn, mx)      uint8_t  f;
#define CFG_F_F32(f, k, j, g, d)              float    f;
#define CFG_F_F32C(f, k, j, g, d, mn, mx)     float    f;
#define CFG_F_I32M(f, k, j, g, d)             int32_t  f;
    CONFIG_TABLE(CFG_F_STR, CFG_F_SEC, CFG_F_BOOL, CFG_F_I32, CFG_F_I32C, CFG_F_U32,
                 CFG_F_U32C, CFG_F_U8, CFG_F_U8C, CFG_F_F32, CFG_F_F32C, CFG_F_I32M)
#undef CFG_F_STR
#undef CFG_F_SEC
#undef CFG_F_BOOL
#undef CFG_F_I32
#undef CFG_F_I32C
#undef CFG_F_U32
#undef CFG_F_U32C
#undef CFG_F_U8
#undef CFG_F_U8C
#undef CFG_F_F32
#undef CFG_F_F32C
#undef CFG_F_I32M
    image_source_t sources[ALLSKY_MAX_SOURCES];
} allsky_config_t;

/* ---- Lifecycle ---------------------------------------------------------- */

/* Create the mutex, load compiled-in defaults, then load persisted values if a
 * stored configuration exists. On first boot (no stored config) applies the
 * default image sources and cycling defaults and persists them. Safe to call
 * once, early in boot, after nvs_flash_init(). */
esp_err_t app_config_init(void);

/* True if a persisted configuration exists (the dedicated configured marker is
 * present in NVS). Independent of WiFi provisioning state. */
bool app_config_has_stored_config(void);

/* Persist only the groups whose values changed since the last save, then clear
 * dirty state. No flash write occurs when nothing is dirty. Always writes the
 * configured marker when it writes anything. */
esp_err_t app_config_save(void);

/* Bitmask of CFG_GRP_* currently marked dirty. */
uint32_t app_config_dirty_groups(void);

/* Force-mark groups dirty (e.g. after a bulk in-place mutation by the restore
 * engine). */
void app_config_mark_dirty(uint32_t groups);

/* Erase the namespace, restore all defaults, clear WiFi provisioning, then write
 * every group back to storage. */
esp_err_t app_config_factory_reset(void);

/* Thread-safe full copy for readers that want a consistent view of many fields. */
void app_config_snapshot(allsky_config_t *out);

/* Firmware identity string embedded in backup _meta.firmware (git commit hash or
 * "unknown"). */
const char *app_config_firmware_id(void);

/* ---- Generated typed getters / setters ---------------------------------- */
/* Each setter clamps/validates, compares against the current value, and marks
 * the field's dirty group only when the value actually changes; returns true if
 * a change occurred. Setters do NOT persist; call app_config_save() to flush. */

#define CFG_D_STR(f, sz, k, j, g, d)          void app_config_get_##f(char *out, size_t len); \
                                              bool app_config_set_##f(const char *v);
#define CFG_D_SEC(f, sz, k, j, g, d)          CFG_D_STR(f, sz, k, j, g, d)
#define CFG_D_BOOL(f, k, j, g, d)             bool app_config_get_##f(void); bool app_config_set_##f(bool v);
#define CFG_D_I32(f, k, j, g, d)              int32_t  app_config_get_##f(void); bool app_config_set_##f(int32_t v);
#define CFG_D_I32C(f, k, j, g, d, mn, mx)     CFG_D_I32(f, k, j, g, d)
#define CFG_D_U32(f, k, j, g, d)              uint32_t app_config_get_##f(void); bool app_config_set_##f(uint32_t v);
#define CFG_D_U32C(f, k, j, g, d, mn, mx)     CFG_D_U32(f, k, j, g, d)
#define CFG_D_U8(f, k, j, g, d)               uint8_t  app_config_get_##f(void); bool app_config_set_##f(uint8_t v);
#define CFG_D_U8C(f, k, j, g, d, mn, mx)      CFG_D_U8(f, k, j, g, d)
#define CFG_D_F32(f, k, j, g, d)              float    app_config_get_##f(void); bool app_config_set_##f(float v);
#define CFG_D_F32C(f, k, j, g, d, mn, mx)     CFG_D_F32(f, k, j, g, d)
/* Manual-setter fields: generated getter, hand-written validated setter. */
#define CFG_D_I32M(f, k, j, g, d)             int32_t  app_config_get_##f(void); bool app_config_set_##f(int32_t v);
CONFIG_TABLE(CFG_D_STR, CFG_D_SEC, CFG_D_BOOL, CFG_D_I32, CFG_D_I32C, CFG_D_U32,
             CFG_D_U32C, CFG_D_U8, CFG_D_U8C, CFG_D_F32, CFG_D_F32C, CFG_D_I32M)
#undef CFG_D_STR
#undef CFG_D_SEC
#undef CFG_D_BOOL
#undef CFG_D_I32
#undef CFG_D_I32C
#undef CFG_D_U32
#undef CFG_D_U32C
#undef CFG_D_U8
#undef CFG_D_U8C
#undef CFG_D_F32
#undef CFG_D_F32C
#undef CFG_D_I32M

/* ---- Image source list -------------------------------------------------- */

/* Current configured source count (1..10). */
int app_config_source_count(void);

/* Copy slot `index` into `out`. Returns false (and leaves *out untouched) if the
 * index is out of range. */
bool app_config_get_source(int index, image_source_t *out);

/* Read the URL of slot `index` into `out`. Out-of-range or >= count yields an
 * empty string and a warning. */
void app_config_get_source_url(int index, char *out, size_t len);

/* Enabled flag for a slot; out-of-range reads as true (legacy semantics). */
bool app_config_get_source_enabled(int index);

/* Duration (seconds) for a slot; out-of-range reads as 30. */
uint32_t app_config_get_source_duration(int index);

/* Resolve the URL to display now: with cycling on and count>0, the URL at the
 * current index (falling back to the legacy single URL if invalid); otherwise
 * the legacy single URL. */
void app_config_current_image_url(char *out, size_t len);

/* Append a source at the current count (if count<10): enabled=true,
 * duration=defaultImageDuration, transform=global defaults. Marks IMAGE and
 * TRANSFORMS dirty. Returns the new index, or -1 if full. */
int app_config_add_source(const char *url);

/* Remove slot `index`. Rejected if out of range or if it would leave zero
 * sources. Entries above shift down (url, transform, enabled AND duration all
 * shift). The freed last slot is reset. currentImageIndex resets to 0 if it is
 * now out of range. Marks IMAGE, TRANSFORMS, CYCLING dirty. */
bool app_config_remove_source(int index);

/* Clear all 10 slots (url empty, enabled true), count=0, currentImageIndex=0.
 * Storage-layer clear. Marks IMAGE and CYCLING dirty. */
void app_config_clear_sources(void);

/* Replace a slot's URL (bounded copy). Marks IMAGE dirty. Out-of-range ignored. */
bool app_config_set_source_url(int index, const char *url);
bool app_config_set_source_enabled(int index, bool enabled);
bool app_config_set_source_duration(int index, uint32_t duration_s);
/* Per-image transform setters clamp scale [0.1,2.0] and snap rotation to
 * 0/90/180/270. Mark TRANSFORMS dirty. */
bool app_config_set_source_transform(int index, const image_transform_t *tf);

/* Build a shuffled permutation of the source indices [0, count) into `out`
 * (Fisher-Yates). Returns the count written (<= max). Used by the cycling
 * scheduler when randomOrder is enabled. */
int app_config_build_shuffle_order(int *out, int max);

/* ---- Backup / restore support (called by config_backup.c) --------------- */

/* Apply every recognized scalar backup key present in `cfg_obj` (a cJSON object)
 * through the validating setters. Secrets are applied only when present and
 * non-empty. currentImageIndex and displayType are NOT applied here (the restore
 * engine sequences them). Returns the number of scalar keys applied. `cfg_obj`
 * is a `const cJSON *` (opaque here to keep this header cJSON-free). */
int app_config_restore_apply_scalars(const void *cfg_obj);

/* True if `key` is a recognized backup key (any table json key, imageSources, or
 * currentImageIndex). Lets the restore engine count unknown keys as skipped. */
bool app_config_is_known_backup_key(const char *key);

/* Fill `cfg_obj` (a cJSON object) with every scalar backup key and the
 * imageSources array. Secret keys are emitted only when include_secrets is true.
 * `cfg_obj` is a `cJSON *` (opaque here). */
void app_config_backup_serialize(void *cfg_obj, bool include_secrets);

#ifdef __cplusplus
}
#endif
