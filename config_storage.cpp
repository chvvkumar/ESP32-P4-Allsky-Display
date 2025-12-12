#include "config_storage.h"
#include "config.h"

// Global instance
ConfigStorage configStorage;

// Storage namespace
const char *ConfigStorage::NAMESPACE = "allsky_config";

ConfigStorage::ConfigStorage() : _dirty(false) {}

bool ConfigStorage::begin() {
  setDefaults();
  loadConfig();
  return true;
}

void ConfigStorage::setDefaults() {
  // Set hardcoded defaults from original config.cpp
  // WiFi credentials are intentionally empty - device will start captive portal
  // on first boot
  config.wifiProvisioned = false;
  config.wifiSSID = "";
  config.wifiPassword = "";

  config.mqttServer = "192.168.1.250";
  config.mqttPort = 1883;
  config.mqttUser = "";
  config.mqttPassword = "";
  config.mqttClientID = "ESP32_Allsky_Display";

  // Home Assistant Discovery defaults
  config.haDiscoveryEnabled = true;
  config.haDeviceName = "ESP32 AllSky Display";
  config.haDiscoveryPrefix = "homeassistant";
  config.haStateTopic = "allsky_display";
  config.haSensorUpdateInterval = 30; // 30 seconds

  config.imageURL = "http://allskypi5.lan/current/resized/image.jpg";

  // Multi-image cycling defaults using external config arrays
  config.cyclingEnabled = DEFAULT_CYCLING_ENABLED;
  config.cycleInterval = DEFAULT_CYCLE_INTERVAL;
  config.randomOrder = DEFAULT_RANDOM_ORDER;
  config.currentImageIndex = 0;
  config.imageSourceCount = DEFAULT_IMAGE_SOURCE_COUNT;

  // Initialize with default image sources from config.cpp
  for (int i = 0; i < DEFAULT_IMAGE_SOURCE_COUNT && i < 10; i++) {
    config.imageSources[i] = String(DEFAULT_IMAGE_SOURCES[i]);
  }
  // Clear remaining slots
  for (int i = DEFAULT_IMAGE_SOURCE_COUNT; i < 10; i++) {
    config.imageSources[i] = "";
  }

  // Initialize per-image transformation settings with global defaults
  for (int i = 0; i < 10; i++) {
    config.imageTransforms[i].scaleX = DEFAULT_SCALE_X;
    config.imageTransforms[i].scaleY = DEFAULT_SCALE_Y;
    config.imageTransforms[i].offsetX = DEFAULT_OFFSET_X;
    config.imageTransforms[i].offsetY = DEFAULT_OFFSET_Y;
    config.imageTransforms[i].rotation = DEFAULT_ROTATION;
  }

  config.defaultBrightness = DEFAULT_BRIGHTNESS;
  config.brightnessAutoMode = true; // Default to auto mode (MQTT control)
  config.defaultScaleX = DEFAULT_SCALE_X;
  config.defaultScaleY = DEFAULT_SCALE_Y;
  config.defaultOffsetX = DEFAULT_OFFSET_X;
  config.defaultOffsetY = DEFAULT_OFFSET_Y;
  config.defaultRotation = DEFAULT_ROTATION;
  config.backlightFreq = BACKLIGHT_FREQ;
  config.backlightResolution = BACKLIGHT_RESOLUTION;

  config.updateInterval = UPDATE_INTERVAL;
  config.mqttReconnectInterval = MQTT_RECONNECT_INTERVAL;
  config.watchdogTimeout = WATCHDOG_TIMEOUT_MS;
  config.criticalHeapThreshold = CRITICAL_HEAP_THRESHOLD;
  config.criticalPSRAMThreshold = CRITICAL_PSRAM_THRESHOLD;

  // Logging defaults
  config.minLogSeverity = DEFAULT_LOG_LEVEL;

  // Time settings
  config.ntpServer = "pool.ntp.org";
  config.timezone = "CST6CDT,M3.2.0,M11.1.0"; // US Central Time by default
  config.ntpEnabled = true;
  
  // Home Assistant REST Control defaults
  config.haBaseUrl = "http://homeassistant.local:8123";
  config.haAccessToken = "";
  config.haLightSensorEntity = "sensor.foyer_night_light_illuminance";
  config.lightSensorMinLux = 0.0;
  config.lightSensorMaxLux = 300.0;
  config.displayMinBrightness = 10;
  config.displayMaxBrightness = 100;
  config.useHaRestControl = false;
  config.haPollInterval = 60;  // 60 seconds
  config.lightSensorMappingMode = 1;  // Logarithmic by default

  _dirty = false; // Defaults are set, but not considered a "change" from saved
                  // state until modified
}

void ConfigStorage::loadConfig() {
  preferences.begin(NAMESPACE, true); // Read-only mode

  if (preferences.isKey("wifi_ssid")) {
    config.wifiProvisioned =
        preferences.getBool("wifi_prov", config.wifiProvisioned);
    config.wifiSSID = preferences.getString("wifi_ssid", config.wifiSSID);
    config.wifiPassword =
        preferences.getString("wifi_pwd", config.wifiPassword);

    config.mqttServer = preferences.getString("mqtt_server", config.mqttServer);
    config.mqttPort = preferences.getInt("mqtt_port", config.mqttPort);
    config.mqttUser = preferences.getString("mqtt_user", config.mqttUser);
    config.mqttPassword =
        preferences.getString("mqtt_pwd", config.mqttPassword);
    config.mqttClientID =
        preferences.getString("mqtt_client", config.mqttClientID);

    // Load Home Assistant Discovery settings
    config.haDiscoveryEnabled =
        preferences.getBool("ha_disc_en", config.haDiscoveryEnabled);
    config.haDeviceName =
        preferences.getString("ha_dev_name", config.haDeviceName);
    config.haDiscoveryPrefix =
        preferences.getString("ha_disc_pfx", config.haDiscoveryPrefix);
    config.haStateTopic =
        preferences.getString("ha_state_top", config.haStateTopic);
    config.haSensorUpdateInterval =
        preferences.getULong("ha_sens_int", config.haSensorUpdateInterval);

    config.imageURL = preferences.getString("image_url", config.imageURL);

    // Load multi-image cycling settings
    config.cyclingEnabled =
        preferences.getBool("cycling_en", config.cyclingEnabled);
    config.cycleInterval =
        preferences.getULong("cycle_intv", config.cycleInterval);
    config.randomOrder = preferences.getBool("random_ord", config.randomOrder);
    config.currentImageIndex =
        preferences.getInt("curr_img_idx", config.currentImageIndex);
    config.imageSourceCount =
        preferences.getInt("img_src_cnt", config.imageSourceCount);

    // Load image sources array
    for (int i = 0; i < 10; i++) {
      String key = "img_src_" + String(i);
      config.imageSources[i] =
          preferences.getString(key.c_str(), config.imageSources[i]);
    }

    // Load per-image transform settings
    for (int i = 0; i < 10; i++) {
      String prefix = "img_tf_" + String(i) + "_";
      config.imageTransforms[i].scaleX = preferences.getFloat(
          (prefix + "sx").c_str(), config.imageTransforms[i].scaleX);
      config.imageTransforms[i].scaleY = preferences.getFloat(
          (prefix + "sy").c_str(), config.imageTransforms[i].scaleY);
      config.imageTransforms[i].offsetX = preferences.getInt(
          (prefix + "ox").c_str(), config.imageTransforms[i].offsetX);
      config.imageTransforms[i].offsetY = preferences.getInt(
          (prefix + "oy").c_str(), config.imageTransforms[i].offsetY);
      config.imageTransforms[i].rotation = preferences.getFloat(
          (prefix + "rot").c_str(), config.imageTransforms[i].rotation);
    }

    config.defaultBrightness =
        preferences.getInt("def_bright", config.defaultBrightness);
    config.brightnessAutoMode =
        preferences.getBool("bright_auto", config.brightnessAutoMode);
    config.defaultScaleX =
        preferences.getFloat("def_scale_x", config.defaultScaleX);
    config.defaultScaleY =
        preferences.getFloat("def_scale_y", config.defaultScaleY);
    config.defaultOffsetX =
        preferences.getInt("def_off_x", config.defaultOffsetX);
    config.defaultOffsetY =
        preferences.getInt("def_off_y", config.defaultOffsetY);
    config.defaultRotation =
        preferences.getFloat("def_rot", config.defaultRotation);
    config.backlightFreq = preferences.getInt("bl_freq", config.backlightFreq);
    config.backlightResolution =
        preferences.getInt("bl_res", config.backlightResolution);

    config.updateInterval =
        preferences.getULong("upd_interval", config.updateInterval);
    config.mqttReconnectInterval =
        preferences.getULong("mqtt_recon", config.mqttReconnectInterval);
    config.watchdogTimeout =
        preferences.getULong("wd_timeout", config.watchdogTimeout);
    config.criticalHeapThreshold =
        preferences.getULong("heap_thresh", config.criticalHeapThreshold);
    config.criticalPSRAMThreshold =
        preferences.getULong("psram_thresh", config.criticalPSRAMThreshold);

    // Load logging settings
    config.minLogSeverity =
        preferences.getInt("log_min_sev", config.minLogSeverity);

    // Load time settings
    config.ntpServer = preferences.getString("ntp_server", config.ntpServer);
    config.timezone = preferences.getString("timezone", config.timezone);
    config.ntpEnabled = preferences.getBool("ntp_enabled", config.ntpEnabled);
    
    // Load Home Assistant REST Control settings
    config.haBaseUrl = preferences.getString("ha_base_url", config.haBaseUrl);
    config.haAccessToken = preferences.getString("ha_token", config.haAccessToken);
    config.haLightSensorEntity = preferences.getString("ha_sensor_ent", config.haLightSensorEntity);
    config.lightSensorMinLux = preferences.getFloat("sensor_min_lux", config.lightSensorMinLux);
    config.lightSensorMaxLux = preferences.getFloat("sensor_max_lux", config.lightSensorMaxLux);
    config.displayMinBrightness = preferences.getInt("disp_min_br", config.displayMinBrightness);
    config.displayMaxBrightness = preferences.getInt("disp_max_br", config.displayMaxBrightness);
    config.useHaRestControl = preferences.getBool("use_ha_rest", config.useHaRestControl);
    config.haPollInterval = preferences.getULong("ha_poll_int", config.haPollInterval);
    config.lightSensorMappingMode = preferences.getInt("sensor_map_mode", config.lightSensorMappingMode);
  }

  preferences.end();
  _dirty = false; // Start fresh after load
}

void ConfigStorage::saveConfig() {
  if (!_dirty) {
    Serial.println("ConfigStorage: No changes to save (skipping flash write)");
    return;
  }

  preferences.begin(NAMESPACE, false); // Read-write mode

  preferences.putBool("wifi_prov", config.wifiProvisioned);
  preferences.putString("wifi_ssid", config.wifiSSID);
  preferences.putString("wifi_pwd", config.wifiPassword);

  preferences.putString("mqtt_server", config.mqttServer);
  preferences.putInt("mqtt_port", config.mqttPort);
  preferences.putString("mqtt_user", config.mqttUser);
  preferences.putString("mqtt_pwd", config.mqttPassword);
  preferences.putString("mqtt_client", config.mqttClientID);

  // Save Home Assistant Discovery settings
  preferences.putBool("ha_disc_en", config.haDiscoveryEnabled);
  preferences.putString("ha_dev_name", config.haDeviceName);
  preferences.putString("ha_disc_pfx", config.haDiscoveryPrefix);
  preferences.putString("ha_state_top", config.haStateTopic);
  preferences.putULong("ha_sens_int", config.haSensorUpdateInterval);

  preferences.putString("image_url", config.imageURL);

  // Save multi-image cycling settings
  preferences.putBool("cycling_en", config.cyclingEnabled);
  preferences.putULong("cycle_intv", config.cycleInterval);
  preferences.putBool("random_ord", config.randomOrder);
  preferences.putInt("curr_img_idx", config.currentImageIndex);
  preferences.putInt("img_src_cnt", config.imageSourceCount);

  // Save image sources array
  for (int i = 0; i < 10; i++) {
    String key = "img_src_" + String(i);
    preferences.putString(key.c_str(), config.imageSources[i]);
  }

  // Save per-image transform settings
  for (int i = 0; i < 10; i++) {
    String prefix = "img_tf_" + String(i) + "_";
    preferences.putFloat((prefix + "sx").c_str(),
                         config.imageTransforms[i].scaleX);
    preferences.putFloat((prefix + "sy").c_str(),
                         config.imageTransforms[i].scaleY);
    preferences.putInt((prefix + "ox").c_str(),
                       config.imageTransforms[i].offsetX);
    preferences.putInt((prefix + "oy").c_str(),
                       config.imageTransforms[i].offsetY);
    preferences.putFloat((prefix + "rot").c_str(),
                         config.imageTransforms[i].rotation);
  }

  preferences.putInt("def_bright", config.defaultBrightness);
  preferences.putBool("bright_auto", config.brightnessAutoMode);
  preferences.putFloat("def_scale_x", config.defaultScaleX);
  preferences.putFloat("def_scale_y", config.defaultScaleY);
  preferences.putInt("def_off_x", config.defaultOffsetX);
  preferences.putInt("def_off_y", config.defaultOffsetY);
  preferences.putFloat("def_rot", config.defaultRotation);
  preferences.putInt("bl_freq", config.backlightFreq);
  preferences.putInt("bl_res", config.backlightResolution);

  preferences.putULong("upd_interval", config.updateInterval);
  preferences.putULong("mqtt_recon", config.mqttReconnectInterval);
  preferences.putULong("wd_timeout", config.watchdogTimeout);
  preferences.putULong("heap_thresh", config.criticalHeapThreshold);
  preferences.putULong("psram_thresh", config.criticalPSRAMThreshold);

  // Save logging settings
  preferences.putInt("log_min_sev", config.minLogSeverity);

  // Save time settings
  preferences.putString("ntp_server", config.ntpServer);
  preferences.putString("timezone", config.timezone);
  preferences.putBool("ntp_enabled", config.ntpEnabled);
  
  // Save Home Assistant REST Control settings
  preferences.putString("ha_base_url", config.haBaseUrl);
  preferences.putString("ha_token", config.haAccessToken);
  preferences.putString("ha_sensor_ent", config.haLightSensorEntity);
  preferences.putFloat("sensor_min_lux", config.lightSensorMinLux);
  preferences.putFloat("sensor_max_lux", config.lightSensorMaxLux);
  preferences.putInt("disp_min_br", config.displayMinBrightness);
  preferences.putInt("disp_max_br", config.displayMaxBrightness);
  preferences.putBool("use_ha_rest", config.useHaRestControl);
  preferences.putULong("ha_poll_int", config.haPollInterval);
  preferences.putInt("sensor_map_mode", config.lightSensorMappingMode);

  preferences.end();
}

void ConfigStorage::resetToDefaults() {
  preferences.begin(NAMESPACE, false);
  preferences.clear();
  preferences.end();

  setDefaults();
  saveConfig();
}

bool ConfigStorage::hasStoredConfig() {
  preferences.begin(NAMESPACE, true);
  bool hasConfig = preferences.isKey("wifi_ssid");
  preferences.end();
  return hasConfig;
}

// Setters
bool ConfigStorage::isWiFiProvisioned() { return config.wifiProvisioned; }
void ConfigStorage::setWiFiProvisioned(bool provisioned) {
  if (config.wifiProvisioned != provisioned) {
    config.wifiProvisioned = provisioned;
    _dirty = true;
  }
}
void ConfigStorage::setWiFiSSID(const String &ssid) {
  if (config.wifiSSID != ssid) {
    config.wifiSSID = ssid;
    _dirty = true;
  }
}
void ConfigStorage::setWiFiPassword(const String &password) {
  if (config.wifiPassword != password) {
    config.wifiPassword = password;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTServer(const String &server) {
  if (config.mqttServer != server) {
    config.mqttServer = server;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTPort(int port) {
  if (config.mqttPort != port) {
    config.mqttPort = port;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTUser(const String &user) {
  if (config.mqttUser != user) {
    config.mqttUser = user;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTPassword(const String &password) {
  if (config.mqttPassword != password) {
    config.mqttPassword = password;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTClientID(const String &clientId) {
  if (config.mqttClientID != clientId) {
    config.mqttClientID = clientId;
    _dirty = true;
  }
}
void ConfigStorage::setImageURL(const String &url) {
  if (config.imageURL != url) {
    config.imageURL = url;
    _dirty = true;
  }
}

// Home Assistant Discovery setters
// Home Assistant Discovery setters
void ConfigStorage::setHADiscoveryEnabled(bool enabled) {
  if (config.haDiscoveryEnabled != enabled) {
    config.haDiscoveryEnabled = enabled;
    _dirty = true;
  }
}
void ConfigStorage::setHADeviceName(const String &name) {
  if (config.haDeviceName != name) {
    config.haDeviceName = name;
    _dirty = true;
  }
}
void ConfigStorage::setHADiscoveryPrefix(const String &prefix) {
  if (config.haDiscoveryPrefix != prefix) {
    config.haDiscoveryPrefix = prefix;
    _dirty = true;
  }
}
void ConfigStorage::setHAStateTopic(const String &topic) {
  if (config.haStateTopic != topic) {
    config.haStateTopic = topic;
    _dirty = true;
  }
}
void ConfigStorage::setHASensorUpdateInterval(unsigned long interval) {
  unsigned long newInterval =
      constrain(interval, 10UL, 300UL); // 10-300 seconds
  if (config.haSensorUpdateInterval != newInterval) {
    config.haSensorUpdateInterval = newInterval;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultBrightness(int brightness) {
  if (config.defaultBrightness != brightness) {
    config.defaultBrightness = brightness;
    _dirty = true;
  }
}
void ConfigStorage::setBrightnessAutoMode(bool autoMode) {
  if (config.brightnessAutoMode != autoMode) {
    config.brightnessAutoMode = autoMode;
    _dirty = true;
  }
}
void ConfigStorage::setUpdateInterval(unsigned long interval) {
  if (config.updateInterval != interval) {
    config.updateInterval = interval;
    _dirty = true;
  }
}
void ConfigStorage::setMQTTReconnectInterval(unsigned long interval) {
  if (config.mqttReconnectInterval != interval) {
    config.mqttReconnectInterval = interval;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultScaleX(float scale) {
  if (config.defaultScaleX != scale) {
    config.defaultScaleX = scale;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultScaleY(float scale) {
  if (config.defaultScaleY != scale) {
    config.defaultScaleY = scale;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultOffsetX(int offset) {
  if (config.defaultOffsetX != offset) {
    config.defaultOffsetX = offset;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultOffsetY(int offset) {
  if (config.defaultOffsetY != offset) {
    config.defaultOffsetY = offset;
    _dirty = true;
  }
}
void ConfigStorage::setDefaultRotation(float rotation) {
  if (config.defaultRotation != rotation) {
    config.defaultRotation = rotation;
    _dirty = true;
  }
}
void ConfigStorage::setBacklightFreq(int freq) {
  if (config.backlightFreq != freq) {
    config.backlightFreq = freq;
    _dirty = true;
  }
}
void ConfigStorage::setBacklightResolution(int resolution) {
  if (config.backlightResolution != resolution) {
    config.backlightResolution = resolution;
    _dirty = true;
  }
}
void ConfigStorage::setWatchdogTimeout(unsigned long timeout) {
  if (config.watchdogTimeout != timeout) {
    config.watchdogTimeout = timeout;
    _dirty = true;
  }
}
void ConfigStorage::setCriticalHeapThreshold(size_t threshold) {
  if (config.criticalHeapThreshold != threshold) {
    config.criticalHeapThreshold = threshold;
    _dirty = true;
  }
}
void ConfigStorage::setCriticalPSRAMThreshold(size_t threshold) {
  if (config.criticalPSRAMThreshold != threshold) {
    config.criticalPSRAMThreshold = threshold;
    _dirty = true;
  }
}

// Getters
String ConfigStorage::getWiFiSSID() { return config.wifiSSID; }
String ConfigStorage::getWiFiPassword() { return config.wifiPassword; }
String ConfigStorage::getMQTTServer() { return config.mqttServer; }
int ConfigStorage::getMQTTPort() { return config.mqttPort; }
String ConfigStorage::getMQTTUser() { return config.mqttUser; }
String ConfigStorage::getMQTTPassword() { return config.mqttPassword; }
String ConfigStorage::getMQTTClientID() { return config.mqttClientID; }
String ConfigStorage::getImageURL() { return config.imageURL; }

// Home Assistant Discovery getters
bool ConfigStorage::getHADiscoveryEnabled() {
  return config.haDiscoveryEnabled;
}
String ConfigStorage::getHADeviceName() { return config.haDeviceName; }
String ConfigStorage::getHADiscoveryPrefix() {
  return config.haDiscoveryPrefix;
}
String ConfigStorage::getHAStateTopic() { return config.haStateTopic; }
unsigned long ConfigStorage::getHASensorUpdateInterval() {
  return config.haSensorUpdateInterval;
}
int ConfigStorage::getDefaultBrightness() { return config.defaultBrightness; }
bool ConfigStorage::getBrightnessAutoMode() {
  return config.brightnessAutoMode;
}
unsigned long ConfigStorage::getUpdateInterval() {
  return config.updateInterval;
}
unsigned long ConfigStorage::getMQTTReconnectInterval() {
  return config.mqttReconnectInterval;
}
float ConfigStorage::getDefaultScaleX() { return config.defaultScaleX; }
float ConfigStorage::getDefaultScaleY() { return config.defaultScaleY; }
int ConfigStorage::getDefaultOffsetX() { return config.defaultOffsetX; }
int ConfigStorage::getDefaultOffsetY() { return config.defaultOffsetY; }
float ConfigStorage::getDefaultRotation() { return config.defaultRotation; }
int ConfigStorage::getBacklightFreq() { return config.backlightFreq; }
int ConfigStorage::getBacklightResolution() {
  return config.backlightResolution;
}
unsigned long ConfigStorage::getWatchdogTimeout() {
  return config.watchdogTimeout;
}
size_t ConfigStorage::getCriticalHeapThreshold() {
  return config.criticalHeapThreshold;
}
size_t ConfigStorage::getCriticalPSRAMThreshold() {
  return config.criticalPSRAMThreshold;
}

// Multi-image cycling setters
void ConfigStorage::setCyclingEnabled(bool enabled) {
  if (config.cyclingEnabled != enabled) {
    config.cyclingEnabled = enabled;
    _dirty = true;
  }
}

void ConfigStorage::setCycleInterval(unsigned long interval) {
  unsigned long newInterval =
      constrain(interval, MIN_CYCLE_INTERVAL, MAX_CYCLE_INTERVAL);
  if (config.cycleInterval != newInterval) {
    config.cycleInterval = newInterval;
    _dirty = true;
  }
}

void ConfigStorage::setRandomOrder(bool random) {
  if (config.randomOrder != random) {
    config.randomOrder = random;
    _dirty = true;
  }
}

void ConfigStorage::setCurrentImageIndex(int index) {
  if (index >= 0 && index < config.imageSourceCount) {
    if (config.currentImageIndex != index) {
      config.currentImageIndex = index;
      _dirty = true;
    }
  }
}

void ConfigStorage::setImageSourceCount(int count) {
  int newCount = constrain(count, 1, 10);
  if (config.imageSourceCount != newCount) {
    config.imageSourceCount = newCount;
    _dirty = true;
  }
}

void ConfigStorage::setImageSource(int index, const String &url) {
  if (index >= 0 && index < 10) {
    if (config.imageSources[index] != url) {
      config.imageSources[index] = url;
      _dirty = true;
    }
  }
}

void ConfigStorage::addImageSource(const String &url) {
  if (config.imageSourceCount < 10) {
    config.imageSources[config.imageSourceCount] = url;
    config.imageSourceCount++;
    _dirty = true;
  }
}

bool ConfigStorage::removeImageSource(int index) {
  // Validate index range
  if (index < 0 || index >= config.imageSourceCount) {
    Serial.printf("ERROR: Invalid index %d (count: %d)\n", index,
                  config.imageSourceCount);
    return false;
  }

  // Prevent removing the last source
  if (config.imageSourceCount <= 1) {
    Serial.printf("ERROR: Cannot remove last image source (count: %d)\n",
                  config.imageSourceCount);
    return false;
  }

  Serial.printf("Removing image source at index %d (count: %d)\n", index,
                config.imageSourceCount);

  // Shift all sources and their transforms after the removed index
  for (int i = index; i < config.imageSourceCount - 1; i++) {
    config.imageSources[i] = config.imageSources[i + 1];
    config.imageTransforms[i] = config.imageTransforms[i + 1];
  }

  // Clear the last source and reset its transform to defaults
  config.imageSources[config.imageSourceCount - 1] = "";
  config.imageTransforms[config.imageSourceCount - 1].scaleX = DEFAULT_SCALE_X;
  config.imageTransforms[config.imageSourceCount - 1].scaleY = DEFAULT_SCALE_Y;
  config.imageTransforms[config.imageSourceCount - 1].offsetX =
      DEFAULT_OFFSET_X;
  config.imageTransforms[config.imageSourceCount - 1].offsetY =
      DEFAULT_OFFSET_Y;
  config.imageTransforms[config.imageSourceCount - 1].rotation =
      DEFAULT_ROTATION;
  config.imageSourceCount--;

  // Adjust current index if necessary
  if (config.currentImageIndex >= config.imageSourceCount) {
    config.currentImageIndex = 0;
  }

  Serial.printf("Image source removed. New count: %d\n",
                config.imageSourceCount);
  _dirty = true;
  return true;
}

void ConfigStorage::clearImageSources() {
  for (int i = 0; i < 10; i++) {
    config.imageSources[i] = "";
  }
  config.imageSourceCount = 0;
  config.currentImageIndex = 0;
  _dirty = true;
}

// Multi-image cycling getters
bool ConfigStorage::getCyclingEnabled() { return config.cyclingEnabled; }

unsigned long ConfigStorage::getCycleInterval() { return config.cycleInterval; }

bool ConfigStorage::getRandomOrder() { return config.randomOrder; }

int ConfigStorage::getCurrentImageIndex() { return config.currentImageIndex; }

int ConfigStorage::getImageSourceCount() { return config.imageSourceCount; }

String ConfigStorage::getImageSource(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageSource index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return "";
  }
  if (index >= config.imageSourceCount) {
    Serial.printf("WARNING: getImageSource index %d >= count %d\n", index, config.imageSourceCount);
    return "";
  }
  return config.imageSources[index];
}

String ConfigStorage::getCurrentImageURL() {
  if (config.cyclingEnabled && config.imageSourceCount > 0) {
    return getImageSource(config.currentImageIndex);
  } else {
    // Fallback to legacy single image URL
    return config.imageURL;
  }
}

String ConfigStorage::getAllImageSources() {
  String json = "[";
  for (int i = 0; i < config.imageSourceCount; i++) {
    if (i > 0)
      json += ",";
    json += "\"" + config.imageSources[i] + "\"";
  }
  json += "]";
  return json;
}

// Per-image transformation setters
void ConfigStorage::setImageScaleX(int index, float scale) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: setImageScaleX index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  float newScale = constrain(scale, MIN_SCALE, MAX_SCALE);
  if (config.imageTransforms[index].scaleX != newScale) {
    config.imageTransforms[index].scaleX = newScale;
    _dirty = true;
  }
}

void ConfigStorage::setImageScaleY(int index, float scale) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: setImageScaleY index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  float newScale = constrain(scale, MIN_SCALE, MAX_SCALE);
  if (config.imageTransforms[index].scaleY != newScale) {
    config.imageTransforms[index].scaleY = newScale;
    _dirty = true;
  }
}

void ConfigStorage::setImageOffsetX(int index, int offset) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: setImageOffsetX index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  if (config.imageTransforms[index].offsetX != offset) {
    config.imageTransforms[index].offsetX = offset;
    _dirty = true;
  }
}

void ConfigStorage::setImageOffsetY(int index, int offset) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: setImageOffsetY index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  if (config.imageTransforms[index].offsetY != offset) {
    config.imageTransforms[index].offsetY = offset;
    _dirty = true;
  }
}

void ConfigStorage::setImageRotation(int index, float rotation) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: setImageRotation index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  // Normalize rotation to 0, 90, 180, or 270 degrees
  float normalizedRotation = rotation;
  while (normalizedRotation >= 360.0f)
    normalizedRotation -= 360.0f;
  while (normalizedRotation < 0.0f)
    normalizedRotation += 360.0f;

  // Round to nearest 90 degrees for valid rotation values
  int rotationStep = round(normalizedRotation / 90.0f);
  float newRotation = rotationStep * 90.0f;

  if (config.imageTransforms[index].rotation != newRotation) {
    config.imageTransforms[index].rotation = newRotation;
    _dirty = true;
  }
}

void ConfigStorage::copyDefaultsToImageTransform(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: copyDefaultsToImageTransform index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return;
  }
  config.imageTransforms[index].scaleX = config.defaultScaleX;
  config.imageTransforms[index].scaleY = config.defaultScaleY;
  config.imageTransforms[index].offsetX = config.defaultOffsetX;
  config.imageTransforms[index].offsetY = config.defaultOffsetY;
  config.imageTransforms[index].rotation = config.defaultRotation;
  _dirty = true;
}

void ConfigStorage::copyAllDefaultsToImageTransforms() {
  for (int i = 0; i < 10; i++) {
    copyDefaultsToImageTransform(i);
  }
}

// Per-image transformation getters
float ConfigStorage::getImageScaleX(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageScaleX index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return DEFAULT_SCALE_X;
  }
  return config.imageTransforms[index].scaleX;
}

float ConfigStorage::getImageScaleY(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageScaleY index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return DEFAULT_SCALE_Y;
  }
  return config.imageTransforms[index].scaleY;
}

int ConfigStorage::getImageOffsetX(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageOffsetX index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return DEFAULT_OFFSET_X;
  }
  return config.imageTransforms[index].offsetX;
}

int ConfigStorage::getImageOffsetY(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageOffsetY index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return DEFAULT_OFFSET_Y;
  }
  return config.imageTransforms[index].offsetY;
}

float ConfigStorage::getImageRotation(int index) {
  if (index < 0 || index >= MAX_IMAGE_SOURCES) {
    Serial.printf("ERROR: getImageRotation index %d out of bounds [0-%d]\n", index, MAX_IMAGE_SOURCES-1);
    return DEFAULT_ROTATION;
  }
  return config.imageTransforms[index].rotation;
}

String ConfigStorage::getImageTransformsAsJson() {
  String json = "[";
  for (int i = 0; i < config.imageSourceCount; i++) {
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"scaleX\":" + String(config.imageTransforms[i].scaleX, 2) + ",";
    json += "\"scaleY\":" + String(config.imageTransforms[i].scaleY, 2) + ",";
    json += "\"offsetX\":" + String(config.imageTransforms[i].offsetX) + ",";
    json += "\"offsetY\":" + String(config.imageTransforms[i].offsetY) + ",";
    json += "\"rotation\":" + String(config.imageTransforms[i].rotation, 1);
    json += "}";
  }
  json += "]";
  return json;
}

void ConfigStorage::setMinLogSeverity(int severity) {
  if (config.minLogSeverity != severity) {
    config.minLogSeverity = severity;
    _dirty = true;
  }
}

int ConfigStorage::getMinLogSeverity() { return config.minLogSeverity; }

void ConfigStorage::setNTPServer(const String &server) {
  if (config.ntpServer != server) {
    config.ntpServer = server;
    _dirty = true;
  }
}

String ConfigStorage::getNTPServer() { return config.ntpServer; }

void ConfigStorage::setTimezone(const String &tz) {
  if (config.timezone != tz) {
    config.timezone = tz;
    _dirty = true;
  }
}

String ConfigStorage::getTimezone() { return config.timezone; }

void ConfigStorage::setNTPEnabled(bool enabled) {
  if (config.ntpEnabled != enabled) {
    config.ntpEnabled = enabled;
    _dirty = true;
  }
}

bool ConfigStorage::getNTPEnabled() { return config.ntpEnabled; }

// Home Assistant REST Control setters
void ConfigStorage::setHABaseUrl(const String &url) {
  if (config.haBaseUrl != url) {
    config.haBaseUrl = url;
    _dirty = true;
  }
}

void ConfigStorage::setHAAccessToken(const String &token) {
  if (config.haAccessToken != token) {
    config.haAccessToken = token;
    _dirty = true;
  }
}

void ConfigStorage::setHALightSensorEntity(const String &entity) {
  if (config.haLightSensorEntity != entity) {
    config.haLightSensorEntity = entity;
    _dirty = true;
  }
}

void ConfigStorage::setLightSensorMinLux(float minLux) {
  if (config.lightSensorMinLux != minLux) {
    config.lightSensorMinLux = minLux;
    _dirty = true;
  }
}

void ConfigStorage::setLightSensorMaxLux(float maxLux) {
  if (config.lightSensorMaxLux != maxLux) {
    config.lightSensorMaxLux = maxLux;
    _dirty = true;
  }
}

void ConfigStorage::setDisplayMinBrightness(int minBrightness) {
  if (config.displayMinBrightness != minBrightness) {
    config.displayMinBrightness = minBrightness;
    _dirty = true;
  }
}

void ConfigStorage::setDisplayMaxBrightness(int maxBrightness) {
  if (config.displayMaxBrightness != maxBrightness) {
    config.displayMaxBrightness = maxBrightness;
    _dirty = true;
  }
}

void ConfigStorage::setUseHARestControl(bool enabled) {
  if (config.useHaRestControl != enabled) {
    config.useHaRestControl = enabled;
    _dirty = true;
  }
}

void ConfigStorage::setHAPollInterval(unsigned long interval) {
  if (config.haPollInterval != interval) {
    config.haPollInterval = interval;
    _dirty = true;
  }
}

void ConfigStorage::setLightSensorMappingMode(int mode) {
  if (config.lightSensorMappingMode != mode) {
    config.lightSensorMappingMode = mode;
    _dirty = true;
  }
}

// Home Assistant REST Control getters
String ConfigStorage::getHABaseUrl() { return config.haBaseUrl; }
String ConfigStorage::getHAAccessToken() { return config.haAccessToken; }
String ConfigStorage::getHALightSensorEntity() { return config.haLightSensorEntity; }
float ConfigStorage::getLightSensorMinLux() { return config.lightSensorMinLux; }
float ConfigStorage::getLightSensorMaxLux() { return config.lightSensorMaxLux; }
int ConfigStorage::getDisplayMinBrightness() { return config.displayMinBrightness; }
int ConfigStorage::getDisplayMaxBrightness() { return config.displayMaxBrightness; }
bool ConfigStorage::getUseHARestControl() { return config.useHaRestControl; }
unsigned long ConfigStorage::getHAPollInterval() { return config.haPollInterval; }
int ConfigStorage::getLightSensorMappingMode() { return config.lightSensorMappingMode; }
