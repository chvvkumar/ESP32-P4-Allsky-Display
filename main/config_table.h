#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

/* First-boot default for displayType (NVS disp_type). The Kconfig ALLSKY_PANEL_* choice is ONLY
 * this default; the panel is otherwise selected at runtime from persisted config. */
#if CONFIG_ALLSKY_PANEL_4_0
#define ALLSKY_DEFAULT_DISP_TYPE 2
#else
#define ALLSKY_DEFAULT_DISP_TYPE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Single-source settings table.
 *
 * Every persisted scalar setting is one row here. Four generated passes fall out
 * of this one table: the in-RAM struct layout, compiled-in defaults, per-key NVS
 * load/save, range clamping, typed getters/setters, and the backup-JSON
 * serialize/restore mapping. Array/cross-field settings (the 10 image-source
 * slots, current-index post-array validation, secret omission) are hand-written
 * alongside because a flat row cannot express them.
 *
 * Row kinds:
 *   STR (field, size, nvskey, json, grp, def)          bounded string
 *   SEC (field, size, nvskey, json, grp, def)          secret string (omitted from backup unless requested)
 *   BOOL(field, nvskey, json, grp, def)                bool, stored u8
 *   I32 (field, nvskey, json, grp, def)                int32, no clamp
 *   I32C(field, nvskey, json, grp, def, mn, mx)        int32, two-sided clamp
 *   U32 (field, nvskey, json, grp, def)                uint32, no clamp
 *   U32C(field, nvskey, json, grp, def, mn, mx)        uint32, two-sided clamp
 *   U8  (field, nvskey, json, grp, def)                uint8, no clamp
 *   U8C (field, nvskey, json, grp, def, mn, mx)        uint8, two-sided clamp
 *   F32 (field, nvskey, json, grp, def)                float, no clamp
 *   F32C(field, nvskey, json, grp, def, mn, mx)        float, two-sided clamp
 *   I32M(field, nvskey, json, grp, def)                int32 with a hand-written validated setter
 *
 * `json` is the backup-schema-v1 key, or NULL for a field that is not part of the
 * backup document (derived counts, internal migration flags).
 * `grp` is one of the CFG_GRP_* dirty-group bits below.
 */

/* Dirty groups: a save rewrites only the NVS keys of groups marked dirty. */
enum {
    CFG_GRP_WIFI       = 1u << 0,
    CFG_GRP_MQTT       = 1u << 1,
    CFG_GRP_HA_DISC    = 1u << 2,
    CFG_GRP_IMAGE      = 1u << 3,
    CFG_GRP_CYCLING    = 1u << 4,
    CFG_GRP_TRANSFORMS = 1u << 5,
    CFG_GRP_DISPLAY    = 1u << 6,
    CFG_GRP_MOON       = 1u << 7,
    CFG_GRP_ADVANCED   = 1u << 8,
    CFG_GRP_LOGGING    = 1u << 9,
    CFG_GRP_TIME       = 1u << 10,
    CFG_GRP_HA_REST    = 1u << 11,
    CFG_GRP_DEVICE     = 1u << 12,
    CFG_GRP_ALL        = 0x1FFFu,
};

#define CONFIG_TABLE(STR, SEC, BOOL, I32, I32C, U32, U32C, U8, U8C, F32, F32C, I32M) \
    /* -- Device -- */ \
    STR (device_name,   48,  "device_name", "deviceName",             CFG_GRP_DEVICE,  "ESP32 AllSky Display") \
    /* -- Network (WiFi) -- */ \
    BOOL(wifi_prov,          "wifi_prov",   "wifiProvisioned",        CFG_GRP_WIFI,    false) \
    STR (wifi_ssid,     33,  "wifi_ssid",   "wifiSSID",               CFG_GRP_WIFI,    "") \
    SEC (wifi_pwd,      64,  "wifi_pwd",    "wifiPassword",           CFG_GRP_WIFI,    "") \
    /* -- MQTT -- */ \
    STR (mqtt_server,   64,  "mqtt_server", "mqttServer",             CFG_GRP_MQTT,    "192.168.1.250") \
    I32 (mqtt_port,          "mqtt_port",   "mqttPort",               CFG_GRP_MQTT,    1883) \
    STR (mqtt_user,     48,  "mqtt_user",   "mqttUser",               CFG_GRP_MQTT,    "") \
    SEC (mqtt_pwd,      64,  "mqtt_pwd",    "mqttPassword",           CFG_GRP_MQTT,    "") \
    STR (mqtt_client,   48,  "mqtt_client", "mqttClientID",           CFG_GRP_MQTT,    "ESP32_Allsky_Display") \
    /* -- Home Assistant MQTT discovery -- */ \
    BOOL(ha_disc_en,         "ha_disc_en",  "haDiscoveryEnabled",     CFG_GRP_HA_DISC, true) \
    STR (ha_dev_name,   48,  "ha_dev_name", "haDeviceName",           CFG_GRP_HA_DISC, "ESP32 AllSky Display") \
    STR (ha_disc_pfx,   48,  "ha_disc_pfx", "haDiscoveryPrefix",      CFG_GRP_HA_DISC, "homeassistant") \
    STR (ha_state_top,  48,  "ha_state_top","haStateTopic",           CFG_GRP_HA_DISC, "allsky_display") \
    U32C(ha_sens_int,        "ha_sens_int", "haSensorUpdateInterval", CFG_GRP_HA_DISC, 30, 10, 300) \
    /* -- Image (legacy single URL) -- */ \
    STR (image_url,     256, "image_url",   "imageURL",               CFG_GRP_IMAGE,   "http://allskypi5.lan/current/resized/image.jpg") \
    /* -- Multi-image cycling -- */ \
    BOOL(cycling_en,         "cycling_en",  "cyclingEnabled",         CFG_GRP_CYCLING, true) \
    I32 (img_upd_mode,       "img_upd_mode","imageUpdateMode",        CFG_GRP_CYCLING, 0) \
    U32C(cycle_intv,         "cycle_intv",  "cycleInterval",          CFG_GRP_CYCLING, 30000, 10000, 3600000) \
    BOOL(random_ord,         "random_ord",  "randomOrder",            CFG_GRP_CYCLING, false) \
    I32M(curr_img_idx,       "curr_img_idx","currentImageIndex",      CFG_GRP_CYCLING, 0) \
    I32C(img_src_cnt,        "img_src_cnt", NULL,                     CFG_GRP_IMAGE,   5, 1, 10) \
    /* -- Default transforms -- */ \
    F32 (def_scale_x,        "def_scale_x", "defaultScaleX",          CFG_GRP_TRANSFORMS, 1.2f) \
    F32 (def_scale_y,        "def_scale_y", "defaultScaleY",          CFG_GRP_TRANSFORMS, 1.2f) \
    I32 (def_off_x,          "def_off_x",   "defaultOffsetX",         CFG_GRP_TRANSFORMS, 0) \
    I32 (def_off_y,          "def_off_y",   "defaultOffsetY",         CFG_GRP_TRANSFORMS, 0) \
    F32 (def_rot,            "def_rot",     "defaultRotation",        CFG_GRP_TRANSFORMS, 0.0f) \
    U32 (def_img_dur,        "def_img_dur", "defaultImageDuration",   CFG_GRP_TRANSFORMS, 30) \
    /* -- Display -- */ \
    I32 (def_bright,         "def_bright",  "defaultBrightness",      CFG_GRP_DISPLAY, 50) \
    BOOL(bright_auto,        "bright_auto", "brightnessAutoMode",     CFG_GRP_DISPLAY, true) \
    I32 (bl_freq,            "bl_freq",     "backlightFreq",          CFG_GRP_DISPLAY, 5000) \
    I32 (bl_res,             "bl_res",      "backlightResolution",    CFG_GRP_DISPLAY, 10) \
    I32M(disp_type,          "disp_type",   "displayType",            CFG_GRP_DISPLAY, ALLSKY_DEFAULT_DISP_TYPE) \
    I32C(color_temp,         "color_temp",  "colorTemp",              CFG_GRP_DISPLAY, 6500, 2000, 15000) \
    /* -- Moon renderer -- */ \
    F32 (moonLat,            "moonLat",     "moonLat",                CFG_GRP_MOON,    0.0f) \
    F32 (moonLon,            "moonLon",     "moonLon",                CFG_GRP_MOON,    0.0f) \
    I32 (moonBg,             "moonBg",      "moonBgStyle",            CFG_GRP_MOON,    3) \
    U8  (mFlipU,             "mFlipU",      "moonFlipU",              CFG_GRP_MOON,    0) \
    U8  (mFlipV,             "mFlipV",      "moonFlipV",              CFG_GRP_MOON,    0) \
    F32C(mRollOff,           "mRollOff",    "moonRollOffset",         CFG_GRP_MOON,    0.0f, -180.0f, 180.0f) \
    F32C(mYawOff,            "mYawOff",     "moonYawOffset",          CFG_GRP_MOON,    0.0f, -180.0f, 180.0f) \
    F32C(mPitchOff,          "mPitchOff",   "moonPitchOffset",        CFG_GRP_MOON,    0.0f, -90.0f, 90.0f) \
    U8  (mNorthUp,           "mNorthUp",    "moonNorthUp",            CFG_GRP_MOON,    1) \
    U8  (mDragLM,            "mDragLM",     "moonDragLightMode",      CFG_GRP_MOON,    0) \
    U8  (mSpinMode,          "mSpinMode",   "moonSpinMode",           CFG_GRP_MOON,    0) \
    U8C (mSpinRet,           "mSpinRet",    "moonSpinReturnS",        CFG_GRP_MOON,    3, 3, 60) \
    BOOL(mScaleMig,          "mScaleMig",   NULL,                     CFG_GRP_MOON,    false) \
    /* -- Advanced -- */ \
    U32 (upd_interval,       "upd_interval","updateInterval",         CFG_GRP_ADVANCED, 120000) \
    U32 (mqtt_recon,         "mqtt_recon",  "mqttReconnectInterval",  CFG_GRP_ADVANCED, 5000) \
    U32 (wd_timeout,         "wd_timeout",  "watchdogTimeout",        CFG_GRP_ADVANCED, 90000) \
    U32 (heap_thresh,        "heap_thresh", "criticalHeapThreshold",  CFG_GRP_ADVANCED, 50000) \
    U32 (psram_thresh,       "psram_thresh","criticalPSRAMThreshold", CFG_GRP_ADVANCED, 100000) \
    /* -- Logging -- */ \
    I32 (log_min_sev,        "log_min_sev", "minLogSeverity",         CFG_GRP_LOGGING, 0) \
    /* -- Time -- */ \
    STR (ntp_server,    64,  "ntp_server",  "ntpServer",              CFG_GRP_TIME,    "pool.ntp.org") \
    STR (timezone,      64,  "timezone",    "timezone",               CFG_GRP_TIME,    "CST6CDT,M3.2.0,M11.1.0") \
    BOOL(ntp_enabled,        "ntp_enabled", "ntpEnabled",             CFG_GRP_TIME,    true) \
    /* -- Home Assistant REST (ambient-light brightness) -- */ \
    STR (ha_base_url,   128, "ha_base_url", "haBaseUrl",              CFG_GRP_HA_REST, "http://homeassistant.local:8123") \
    SEC (ha_token,      256, "ha_token",    "haAccessToken",          CFG_GRP_HA_REST, "") \
    STR (ha_sensor_ent, 96,  "ha_sensor_ent","haLightSensorEntity",   CFG_GRP_HA_REST, "sensor.foyer_night_light_illuminance") \
    F32 (sensor_min_lux,     "sensor_min_lux","lightSensorMinLux",    CFG_GRP_HA_REST, 0.0f) \
    F32 (sensor_max_lux,     "sensor_max_lux","lightSensorMaxLux",    CFG_GRP_HA_REST, 300.0f) \
    I32 (disp_min_br,        "disp_min_br", "displayMinBrightness",   CFG_GRP_HA_REST, 10) \
    I32 (disp_max_br,        "disp_max_br", "displayMaxBrightness",   CFG_GRP_HA_REST, 100) \
    BOOL(use_ha_rest,        "use_ha_rest", "useHaRestControl",       CFG_GRP_HA_REST, false) \
    U32 (ha_poll_int,        "ha_poll_int", "haPollInterval",         CFG_GRP_HA_REST, 60) \
    I32 (sensor_map_mode,    "sensor_map_mode","lightSensorMappingMode",CFG_GRP_HA_REST, 1)

#ifdef __cplusplus
}
#endif
