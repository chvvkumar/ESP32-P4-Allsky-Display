#include "config_backup.h"
#include "config_storage.h"
#include "config.h"
#include "build_info.h"
#include <ArduinoJson.h>

// This file owns the single source of truth for the backup field list. The
// export and import key lists below are kept adjacent so they stay in sync.
// Any change to ConfigStorage fields should be reflected in BOTH lists and the
// CONFIG_SCHEMA_VERSION constant bumped if the change is non-additive.

namespace ConfigBackup {

// Migration hook for non-additive schema changes (field renames or semantic
// changes). Mutates the parsed `config` object in place before the apply step.
// No-op for version 1: additive changes are handled automatically by the
// lenient apply (unknown keys ignored, absent keys left at current values).
// Example future use: if cycleInterval units change between versions, rewrite
// config["cycleInterval"] here based on fromVersion.
static void migrate(JsonObject config, int fromVersion) {
  (void)config;
  (void)fromVersion;
}

String exportJson(bool includeSecrets) {
  JsonDocument doc;

  JsonObject meta = doc["_meta"].to<JsonObject>();
  meta["schemaVersion"] = CONFIG_SCHEMA_VERSION;
  meta["firmware"] = GIT_COMMIT_HASH;
  meta["deviceName"] = configStorage.getDeviceName();
  meta["displayType"] = configStorage.getDisplayType();
  meta["secretsIncluded"] = includeSecrets;

  JsonObject config = doc["config"].to<JsonObject>();

  // Device
  config["deviceName"] = configStorage.getDeviceName();

  // Network
  config["wifiProvisioned"] = configStorage.isWiFiProvisioned();
  config["wifiSSID"] = configStorage.getWiFiSSID();
  if (includeSecrets) config["wifiPassword"] = configStorage.getWiFiPassword();

  // MQTT
  config["mqttServer"] = configStorage.getMQTTServer();
  config["mqttPort"] = configStorage.getMQTTPort();
  config["mqttUser"] = configStorage.getMQTTUser();
  if (includeSecrets) config["mqttPassword"] = configStorage.getMQTTPassword();
  config["mqttClientID"] = configStorage.getMQTTClientID();

  // Home Assistant Discovery
  config["haDiscoveryEnabled"] = configStorage.getHADiscoveryEnabled();
  config["haDeviceName"] = configStorage.getHADeviceName();
  config["haDiscoveryPrefix"] = configStorage.getHADiscoveryPrefix();
  config["haStateTopic"] = configStorage.getHAStateTopic();
  config["haSensorUpdateInterval"] = configStorage.getHASensorUpdateInterval();

  // Image (legacy single URL)
  config["imageURL"] = configStorage.getImageURL();

  // Multi-image cycling
  config["cyclingEnabled"] = configStorage.getCyclingEnabled();
  config["imageUpdateMode"] = configStorage.getImageUpdateMode();
  config["cycleInterval"] = configStorage.getCycleInterval();
  config["randomOrder"] = configStorage.getRandomOrder();
  config["currentImageIndex"] = configStorage.getCurrentImageIndex();

  // Image sources array
  JsonArray sources = config["imageSources"].to<JsonArray>();
  int count = configStorage.getImageSourceCount();
  for (int i = 0; i < count && i < MAX_IMAGE_SOURCES; i++) {
    JsonObject src = sources.add<JsonObject>();
    src["url"] = configStorage.getImageSource(i);
    src["enabled"] = configStorage.isImageEnabled(i);
    src["duration"] = configStorage.getImageDuration(i);
    src["scaleX"] = configStorage.getImageScaleX(i);
    src["scaleY"] = configStorage.getImageScaleY(i);
    src["offsetX"] = configStorage.getImageOffsetX(i);
    src["offsetY"] = configStorage.getImageOffsetY(i);
    src["rotation"] = configStorage.getImageRotation(i);
  }

  // Display
  config["defaultBrightness"] = configStorage.getDefaultBrightness();
  config["brightnessAutoMode"] = configStorage.getBrightnessAutoMode();
  config["defaultScaleX"] = configStorage.getDefaultScaleX();
  config["defaultScaleY"] = configStorage.getDefaultScaleY();
  config["defaultOffsetX"] = configStorage.getDefaultOffsetX();
  config["defaultOffsetY"] = configStorage.getDefaultOffsetY();
  config["defaultRotation"] = configStorage.getDefaultRotation();
  config["defaultImageDuration"] = configStorage.getDefaultImageDuration();
  config["backlightFreq"] = configStorage.getBacklightFreq();
  config["backlightResolution"] = configStorage.getBacklightResolution();

  // Display hardware
  config["displayType"] = configStorage.getDisplayType();
  config["colorTemp"] = configStorage.getColorTemp();

  // Moon render
  config["moonLat"] = configStorage.getMoonLat();
  config["moonLon"] = configStorage.getMoonLon();
  config["moonBgStyle"] = configStorage.getMoonBgStyle();
  config["moonFlipU"] = configStorage.getMoonFlipU();
  config["moonFlipV"] = configStorage.getMoonFlipV();
  config["moonRollOffset"] = configStorage.getMoonRollOffset();
  config["moonYawOffset"] = configStorage.getMoonYawOffset();
  config["moonPitchOffset"] = configStorage.getMoonPitchOffset();
  config["moonNorthUp"] = configStorage.getMoonNorthUp();
  config["moonDragLightMode"] = configStorage.getMoonDragLightMode();
  config["moonSpinMode"] = configStorage.getMoonSpinMode();
  config["moonSpinReturnS"] = configStorage.getMoonSpinReturnS();

  // Advanced
  config["updateInterval"] = configStorage.getUpdateInterval();
  config["mqttReconnectInterval"] = configStorage.getMQTTReconnectInterval();
  config["watchdogTimeout"] = configStorage.getWatchdogTimeout();
  config["criticalHeapThreshold"] = configStorage.getCriticalHeapThreshold();
  config["criticalPSRAMThreshold"] = configStorage.getCriticalPSRAMThreshold();

  // Logging
  config["minLogSeverity"] = configStorage.getMinLogSeverity();

  // Time
  config["ntpServer"] = configStorage.getNTPServer();
  config["timezone"] = configStorage.getTimezone();
  config["ntpEnabled"] = configStorage.getNTPEnabled();

  // Home Assistant REST control
  config["haBaseUrl"] = configStorage.getHABaseUrl();
  if (includeSecrets) config["haAccessToken"] = configStorage.getHAAccessToken();
  config["haLightSensorEntity"] = configStorage.getHALightSensorEntity();
  config["lightSensorMinLux"] = configStorage.getLightSensorMinLux();
  config["lightSensorMaxLux"] = configStorage.getLightSensorMaxLux();
  config["displayMinBrightness"] = configStorage.getDisplayMinBrightness();
  config["displayMaxBrightness"] = configStorage.getDisplayMaxBrightness();
  config["useHaRestControl"] = configStorage.getUseHARestControl();
  config["haPollInterval"] = configStorage.getHAPollInterval();
  config["lightSensorMappingMode"] = configStorage.getLightSensorMappingMode();

  String out;
  serializeJson(doc, out);
  return out;
}

RestoreResult importJson(const String& body) {
  RestoreResult result;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    result.ok = false;
    result.error = String("JSON parse error: ") + err.c_str();
    return result;
  }

  if (!doc["config"].is<JsonObject>()) {
    result.ok = false;
    result.error = "Missing or invalid 'config' object";
    return result;
  }

  JsonObject meta = doc["_meta"].is<JsonObject>() ? doc["_meta"].as<JsonObject>()
                                                  : JsonObject();
  result.fileVersion = meta["schemaVersion"] | 0;
  result.versionMismatch = (result.fileVersion != CONFIG_SCHEMA_VERSION);
  result.secretsIncluded = meta["secretsIncluded"] | false;

  JsonObject config = doc["config"].as<JsonObject>();

  if (result.fileVersion < CONFIG_SCHEMA_VERSION) {
    migrate(config, result.fileVersion);
  }

  // Track which keys are recognized so anything left over counts as skipped.
  // Each recognized scalar key present in `config` is applied and counted.
  int applied = 0;
  int skipped = 0;

  // Helper macros keep the apply list compact and adjacent to the export list.
  // Each checks presence via the typed is<T>() probe before calling the setter.
  #define APPLY_STR(key, setter) \
    if (config[key].is<const char*>()) { configStorage.setter(config[key].as<String>()); applied++; }
  #define APPLY_BOOL(key, setter) \
    if (config[key].is<bool>()) { configStorage.setter(config[key].as<bool>()); applied++; }
  #define APPLY_INT(key, setter) \
    if (config[key].is<int>()) { configStorage.setter(config[key].as<int>()); applied++; }
  #define APPLY_UL(key, setter) \
    if (config[key].is<unsigned long>() || config[key].is<long>() || config[key].is<int>()) \
      { configStorage.setter(config[key].as<unsigned long>()); applied++; }
  #define APPLY_FLOAT(key, setter) \
    if (config[key].is<float>() || config[key].is<int>()) { configStorage.setter(config[key].as<float>()); applied++; }
  #define APPLY_SIZE(key, setter) \
    if (config[key].is<unsigned long>() || config[key].is<long>() || config[key].is<int>()) \
      { configStorage.setter((size_t)(config[key].as<unsigned long>())); applied++; }
  #define APPLY_U8(key, setter) \
    if (config[key].is<int>()) { configStorage.setter((uint8_t)(config[key].as<int>())); applied++; }

  // Secret string: apply only when present AND non-empty, so a no-secrets
  // backup never erases existing credentials.
  #define APPLY_SECRET(key, setter) \
    if (config[key].is<const char*>()) { \
      String v = config[key].as<String>(); \
      if (v.length() > 0) { configStorage.setter(v); applied++; } \
    }

  // Device
  APPLY_STR("deviceName", setDeviceName);

  // Network
  APPLY_BOOL("wifiProvisioned", setWiFiProvisioned);
  APPLY_STR("wifiSSID", setWiFiSSID);
  APPLY_SECRET("wifiPassword", setWiFiPassword);

  // MQTT
  APPLY_STR("mqttServer", setMQTTServer);
  APPLY_INT("mqttPort", setMQTTPort);
  APPLY_STR("mqttUser", setMQTTUser);
  APPLY_SECRET("mqttPassword", setMQTTPassword);
  APPLY_STR("mqttClientID", setMQTTClientID);

  // Home Assistant Discovery
  APPLY_BOOL("haDiscoveryEnabled", setHADiscoveryEnabled);
  APPLY_STR("haDeviceName", setHADeviceName);
  APPLY_STR("haDiscoveryPrefix", setHADiscoveryPrefix);
  APPLY_STR("haStateTopic", setHAStateTopic);
  APPLY_UL("haSensorUpdateInterval", setHASensorUpdateInterval);

  // Image (legacy single URL)
  APPLY_STR("imageURL", setImageURL);

  // Multi-image cycling
  APPLY_BOOL("cyclingEnabled", setCyclingEnabled);
  APPLY_INT("imageUpdateMode", setImageUpdateMode);
  APPLY_UL("cycleInterval", setCycleInterval);
  APPLY_BOOL("randomOrder", setRandomOrder);
  // NOTE: currentImageIndex is applied AFTER the image sources are rebuilt
  // (clearImageSources resets it to 0 and setCurrentImageIndex validates against
  // the live source count), see below.

  // Display
  APPLY_INT("defaultBrightness", setDefaultBrightness);
  APPLY_BOOL("brightnessAutoMode", setBrightnessAutoMode);
  APPLY_FLOAT("defaultScaleX", setDefaultScaleX);
  APPLY_FLOAT("defaultScaleY", setDefaultScaleY);
  APPLY_INT("defaultOffsetX", setDefaultOffsetX);
  APPLY_INT("defaultOffsetY", setDefaultOffsetY);
  APPLY_FLOAT("defaultRotation", setDefaultRotation);
  APPLY_UL("defaultImageDuration", setDefaultImageDuration);
  APPLY_INT("backlightFreq", setBacklightFreq);
  APPLY_INT("backlightResolution", setBacklightResolution);

  // Display hardware
  APPLY_INT("displayType", setDisplayType);
  APPLY_INT("colorTemp", setColorTemp);

  // Moon render
  APPLY_FLOAT("moonLat", setMoonLat);
  APPLY_FLOAT("moonLon", setMoonLon);
  APPLY_INT("moonBgStyle", setMoonBgStyle);
  APPLY_U8("moonFlipU", setMoonFlipU);
  APPLY_U8("moonFlipV", setMoonFlipV);
  APPLY_FLOAT("moonRollOffset", setMoonRollOffset);
  APPLY_FLOAT("moonYawOffset", setMoonYawOffset);
  APPLY_FLOAT("moonPitchOffset", setMoonPitchOffset);
  APPLY_U8("moonNorthUp", setMoonNorthUp);
  APPLY_U8("moonDragLightMode", setMoonDragLightMode);
  APPLY_U8("moonSpinMode", setMoonSpinMode);
  APPLY_U8("moonSpinReturnS", setMoonSpinReturnS);

  // Advanced
  APPLY_UL("updateInterval", setUpdateInterval);
  APPLY_UL("mqttReconnectInterval", setMQTTReconnectInterval);
  APPLY_UL("watchdogTimeout", setWatchdogTimeout);
  APPLY_SIZE("criticalHeapThreshold", setCriticalHeapThreshold);
  APPLY_SIZE("criticalPSRAMThreshold", setCriticalPSRAMThreshold);

  // Logging
  APPLY_INT("minLogSeverity", setMinLogSeverity);

  // Time
  APPLY_STR("ntpServer", setNTPServer);
  APPLY_STR("timezone", setTimezone);
  APPLY_BOOL("ntpEnabled", setNTPEnabled);

  // Home Assistant REST control
  APPLY_STR("haBaseUrl", setHABaseUrl);
  APPLY_SECRET("haAccessToken", setHAAccessToken);
  APPLY_STR("haLightSensorEntity", setHALightSensorEntity);
  APPLY_FLOAT("lightSensorMinLux", setLightSensorMinLux);
  APPLY_FLOAT("lightSensorMaxLux", setLightSensorMaxLux);
  APPLY_INT("displayMinBrightness", setDisplayMinBrightness);
  APPLY_INT("displayMaxBrightness", setDisplayMaxBrightness);
  APPLY_BOOL("useHaRestControl", setUseHARestControl);
  APPLY_UL("haPollInterval", setHAPollInterval);
  APPLY_INT("lightSensorMappingMode", setLightSensorMappingMode);

  #undef APPLY_STR
  #undef APPLY_BOOL
  #undef APPLY_INT
  #undef APPLY_UL
  #undef APPLY_FLOAT
  #undef APPLY_SIZE
  #undef APPLY_U8
  #undef APPLY_SECRET

  // Image sources: rebuild the array from scratch. Extras beyond
  // MAX_IMAGE_SOURCES are skipped and counted.
  if (config["imageSources"].is<JsonArray>()) {
    JsonArray sources = config["imageSources"].as<JsonArray>();
    configStorage.clearImageSources();
    int idx = 0;
    for (JsonObject src : sources) {
      if (idx >= MAX_IMAGE_SOURCES) {
        skipped++;
        continue;
      }
      String url = src["url"].is<const char*>() ? src["url"].as<String>() : String("");
      configStorage.addImageSource(url);
      if (src["enabled"].is<bool>())   configStorage.setImageEnabled(idx, src["enabled"].as<bool>());
      if (src["duration"].is<int>() || src["duration"].is<unsigned long>() || src["duration"].is<long>())
        configStorage.setImageDuration(idx, src["duration"].as<unsigned long>());
      if (src["scaleX"].is<float>() || src["scaleX"].is<int>())   configStorage.setImageScaleX(idx, src["scaleX"].as<float>());
      if (src["scaleY"].is<float>() || src["scaleY"].is<int>())   configStorage.setImageScaleY(idx, src["scaleY"].as<float>());
      if (src["offsetX"].is<int>())  configStorage.setImageOffsetX(idx, src["offsetX"].as<int>());
      if (src["offsetY"].is<int>())  configStorage.setImageOffsetY(idx, src["offsetY"].as<int>());
      if (src["rotation"].is<float>() || src["rotation"].is<int>()) configStorage.setImageRotation(idx, src["rotation"].as<float>());
      applied++;
      idx++;
    }
  } else {
    // Count an unrecognized imageSources value (e.g. wrong type) as skipped if present.
    if (!config["imageSources"].isNull()) skipped++;
  }

  // Apply currentImageIndex now that the source array (and its count) is rebuilt.
  // setCurrentImageIndex validates the index against the live source count.
  if (config["currentImageIndex"].is<int>()) {
    configStorage.setCurrentImageIndex(config["currentImageIndex"].as<int>());
    applied++;
  }

  // Count any keys in `config` that we did not recognize as skipped.
  // Recognized keys were applied above; everything else is unknown.
  // (Scalar keys with a wrong/absent type that we did not apply also fall here.)
  // We approximate by counting members not in the recognized set.
  static const char* kKnown[] = {
    "deviceName","wifiProvisioned","wifiSSID","wifiPassword",
    "mqttServer","mqttPort","mqttUser","mqttPassword","mqttClientID",
    "haDiscoveryEnabled","haDeviceName","haDiscoveryPrefix","haStateTopic","haSensorUpdateInterval",
    "imageURL","cyclingEnabled","imageUpdateMode","cycleInterval","randomOrder","currentImageIndex",
    "imageSources",
    "defaultBrightness","brightnessAutoMode","defaultScaleX","defaultScaleY","defaultOffsetX",
    "defaultOffsetY","defaultRotation","defaultImageDuration","backlightFreq","backlightResolution",
    "displayType","colorTemp",
    "moonLat","moonLon","moonBgStyle","moonFlipU","moonFlipV","moonRollOffset","moonYawOffset",
    "moonPitchOffset","moonNorthUp","moonDragLightMode","moonSpinMode","moonSpinReturnS",
    "updateInterval","mqttReconnectInterval","watchdogTimeout","criticalHeapThreshold","criticalPSRAMThreshold",
    "minLogSeverity","ntpServer","timezone","ntpEnabled",
    "haBaseUrl","haAccessToken","haLightSensorEntity","lightSensorMinLux","lightSensorMaxLux",
    "displayMinBrightness","displayMaxBrightness","useHaRestControl","haPollInterval","lightSensorMappingMode"
  };
  const int kKnownCount = sizeof(kKnown) / sizeof(kKnown[0]);
  for (JsonPair kv : config) {
    bool known = false;
    for (int i = 0; i < kKnownCount; i++) {
      if (strcmp(kv.key().c_str(), kKnown[i]) == 0) { known = true; break; }
    }
    if (!known) skipped++;
  }

  configStorage.saveConfig();

  result.ok = true;
  result.applied = applied;
  result.skipped = skipped;
  return result;
}

}  // namespace ConfigBackup
