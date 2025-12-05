#include "config_storage.h"
#include "config.h"

// Global instance
ConfigStorage configStorage;

// Storage namespace
const char* ConfigStorage::NAMESPACE = "allsky_config";

ConfigStorage::ConfigStorage() {
}

bool ConfigStorage::begin() {
    setDefaults();
    loadConfig();
    return true;
}

void ConfigStorage::setDefaults() {
    // Set hardcoded defaults from original config.cpp
    // WiFi credentials are empty by default - device will start in AP mode for configuration
    config.wifiConfigured = false;
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
    
    config.imageURL = "https://cdn.star.nesdis.noaa.gov/GOES19/ABI/SECTOR/umv/GEOCOLOR/600x600.jpg";
    
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
}

void ConfigStorage::loadConfig() {
    preferences.begin(NAMESPACE, true); // Read-only mode
    
    if (preferences.isKey("wifi_ssid")) {
        config.wifiConfigured = preferences.getBool("wifi_configured", false);
        config.wifiSSID = preferences.getString("wifi_ssid", config.wifiSSID);
        config.wifiPassword = preferences.getString("wifi_pwd", config.wifiPassword);
        
        config.mqttServer = preferences.getString("mqtt_server", config.mqttServer);
        config.mqttPort = preferences.getInt("mqtt_port", config.mqttPort);
        config.mqttUser = preferences.getString("mqtt_user", config.mqttUser);
        config.mqttPassword = preferences.getString("mqtt_pwd", config.mqttPassword);
        config.mqttClientID = preferences.getString("mqtt_client", config.mqttClientID);
        
        // Load Home Assistant Discovery settings
        config.haDiscoveryEnabled = preferences.getBool("ha_disc_en", config.haDiscoveryEnabled);
        config.haDeviceName = preferences.getString("ha_dev_name", config.haDeviceName);
        config.haDiscoveryPrefix = preferences.getString("ha_disc_pfx", config.haDiscoveryPrefix);
        config.haStateTopic = preferences.getString("ha_state_top", config.haStateTopic);
        config.haSensorUpdateInterval = preferences.getULong("ha_sens_int", config.haSensorUpdateInterval);
        
        config.imageURL = preferences.getString("image_url", config.imageURL);
        
        // Load multi-image cycling settings
        config.cyclingEnabled = preferences.getBool("cycling_en", config.cyclingEnabled);
        config.cycleInterval = preferences.getULong("cycle_intv", config.cycleInterval);
        config.randomOrder = preferences.getBool("random_ord", config.randomOrder);
        config.currentImageIndex = preferences.getInt("curr_img_idx", config.currentImageIndex);
        config.imageSourceCount = preferences.getInt("img_src_cnt", config.imageSourceCount);
        
        // Load image sources array
        for (int i = 0; i < 10; i++) {
            String key = "img_src_" + String(i);
            config.imageSources[i] = preferences.getString(key.c_str(), config.imageSources[i]);
        }
        
        // Load per-image transform settings
        for (int i = 0; i < 10; i++) {
            String prefix = "img_tf_" + String(i) + "_";
            config.imageTransforms[i].scaleX = preferences.getFloat((prefix + "sx").c_str(), config.imageTransforms[i].scaleX);
            config.imageTransforms[i].scaleY = preferences.getFloat((prefix + "sy").c_str(), config.imageTransforms[i].scaleY);
            config.imageTransforms[i].offsetX = preferences.getInt((prefix + "ox").c_str(), config.imageTransforms[i].offsetX);
            config.imageTransforms[i].offsetY = preferences.getInt((prefix + "oy").c_str(), config.imageTransforms[i].offsetY);
            config.imageTransforms[i].rotation = preferences.getFloat((prefix + "rot").c_str(), config.imageTransforms[i].rotation);
        }
        
        config.defaultBrightness = preferences.getInt("def_bright", config.defaultBrightness);
        config.brightnessAutoMode = preferences.getBool("bright_auto", config.brightnessAutoMode);
        config.defaultScaleX = preferences.getFloat("def_scale_x", config.defaultScaleX);
        config.defaultScaleY = preferences.getFloat("def_scale_y", config.defaultScaleY);
        config.defaultOffsetX = preferences.getInt("def_off_x", config.defaultOffsetX);
        config.defaultOffsetY = preferences.getInt("def_off_y", config.defaultOffsetY);
        config.defaultRotation = preferences.getFloat("def_rot", config.defaultRotation);
        config.backlightFreq = preferences.getInt("bl_freq", config.backlightFreq);
        config.backlightResolution = preferences.getInt("bl_res", config.backlightResolution);
        
        config.updateInterval = preferences.getULong("upd_interval", config.updateInterval);
        config.mqttReconnectInterval = preferences.getULong("mqtt_recon", config.mqttReconnectInterval);
        config.watchdogTimeout = preferences.getULong("wd_timeout", config.watchdogTimeout);
        config.criticalHeapThreshold = preferences.getULong("heap_thresh", config.criticalHeapThreshold);
        config.criticalPSRAMThreshold = preferences.getULong("psram_thresh", config.criticalPSRAMThreshold);
    }
    
    preferences.end();
}

void ConfigStorage::saveConfig() {
    preferences.begin(NAMESPACE, false); // Read-write mode
    
    preferences.putBool("wifi_configured", config.wifiConfigured);
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
        preferences.putFloat((prefix + "sx").c_str(), config.imageTransforms[i].scaleX);
        preferences.putFloat((prefix + "sy").c_str(), config.imageTransforms[i].scaleY);
        preferences.putInt((prefix + "ox").c_str(), config.imageTransforms[i].offsetX);
        preferences.putInt((prefix + "oy").c_str(), config.imageTransforms[i].offsetY);
        preferences.putFloat((prefix + "rot").c_str(), config.imageTransforms[i].rotation);
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

bool ConfigStorage::isWiFiConfigured() {
    return config.wifiConfigured;
}

void ConfigStorage::setWiFiConfigured(bool configured) {
    config.wifiConfigured = configured;
}

void ConfigStorage::clearWiFiConfig() {
    config.wifiConfigured = false;
    config.wifiSSID = "";
    config.wifiPassword = "";
    saveConfig();
}

// Setters
void ConfigStorage::setWiFiSSID(const String& ssid) { config.wifiSSID = ssid; }
void ConfigStorage::setWiFiPassword(const String& password) { config.wifiPassword = password; }
void ConfigStorage::setMQTTServer(const String& server) { config.mqttServer = server; }
void ConfigStorage::setMQTTPort(int port) { config.mqttPort = port; }
void ConfigStorage::setMQTTUser(const String& user) { config.mqttUser = user; }
void ConfigStorage::setMQTTPassword(const String& password) { config.mqttPassword = password; }
void ConfigStorage::setMQTTClientID(const String& clientId) { config.mqttClientID = clientId; }
void ConfigStorage::setImageURL(const String& url) { config.imageURL = url; }

// Home Assistant Discovery setters
void ConfigStorage::setHADiscoveryEnabled(bool enabled) { config.haDiscoveryEnabled = enabled; }
void ConfigStorage::setHADeviceName(const String& name) { config.haDeviceName = name; }
void ConfigStorage::setHADiscoveryPrefix(const String& prefix) { config.haDiscoveryPrefix = prefix; }
void ConfigStorage::setHAStateTopic(const String& topic) { config.haStateTopic = topic; }
void ConfigStorage::setHASensorUpdateInterval(unsigned long interval) { 
    config.haSensorUpdateInterval = constrain(interval, 10UL, 300UL); // 10-300 seconds
}
void ConfigStorage::setDefaultBrightness(int brightness) { config.defaultBrightness = brightness; }
void ConfigStorage::setBrightnessAutoMode(bool autoMode) { config.brightnessAutoMode = autoMode; }
void ConfigStorage::setUpdateInterval(unsigned long interval) { config.updateInterval = interval; }
void ConfigStorage::setMQTTReconnectInterval(unsigned long interval) { config.mqttReconnectInterval = interval; }
void ConfigStorage::setDefaultScaleX(float scale) { config.defaultScaleX = scale; }
void ConfigStorage::setDefaultScaleY(float scale) { config.defaultScaleY = scale; }
void ConfigStorage::setDefaultOffsetX(int offset) { config.defaultOffsetX = offset; }
void ConfigStorage::setDefaultOffsetY(int offset) { config.defaultOffsetY = offset; }
void ConfigStorage::setDefaultRotation(float rotation) { config.defaultRotation = rotation; }
void ConfigStorage::setBacklightFreq(int freq) { config.backlightFreq = freq; }
void ConfigStorage::setBacklightResolution(int resolution) { config.backlightResolution = resolution; }
void ConfigStorage::setWatchdogTimeout(unsigned long timeout) { config.watchdogTimeout = timeout; }
void ConfigStorage::setCriticalHeapThreshold(size_t threshold) { config.criticalHeapThreshold = threshold; }
void ConfigStorage::setCriticalPSRAMThreshold(size_t threshold) { config.criticalPSRAMThreshold = threshold; }

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
bool ConfigStorage::getHADiscoveryEnabled() { return config.haDiscoveryEnabled; }
String ConfigStorage::getHADeviceName() { return config.haDeviceName; }
String ConfigStorage::getHADiscoveryPrefix() { return config.haDiscoveryPrefix; }
String ConfigStorage::getHAStateTopic() { return config.haStateTopic; }
unsigned long ConfigStorage::getHASensorUpdateInterval() { return config.haSensorUpdateInterval; }
int ConfigStorage::getDefaultBrightness() { return config.defaultBrightness; }
bool ConfigStorage::getBrightnessAutoMode() { return config.brightnessAutoMode; }
unsigned long ConfigStorage::getUpdateInterval() { return config.updateInterval; }
unsigned long ConfigStorage::getMQTTReconnectInterval() { return config.mqttReconnectInterval; }
float ConfigStorage::getDefaultScaleX() { return config.defaultScaleX; }
float ConfigStorage::getDefaultScaleY() { return config.defaultScaleY; }
int ConfigStorage::getDefaultOffsetX() { return config.defaultOffsetX; }
int ConfigStorage::getDefaultOffsetY() { return config.defaultOffsetY; }
float ConfigStorage::getDefaultRotation() { return config.defaultRotation; }
int ConfigStorage::getBacklightFreq() { return config.backlightFreq; }
int ConfigStorage::getBacklightResolution() { return config.backlightResolution; }
unsigned long ConfigStorage::getWatchdogTimeout() { return config.watchdogTimeout; }
size_t ConfigStorage::getCriticalHeapThreshold() { return config.criticalHeapThreshold; }
size_t ConfigStorage::getCriticalPSRAMThreshold() { return config.criticalPSRAMThreshold; }

// Multi-image cycling setters
void ConfigStorage::setCyclingEnabled(bool enabled) { 
    config.cyclingEnabled = enabled; 
}

void ConfigStorage::setCycleInterval(unsigned long interval) { 
    config.cycleInterval = constrain(interval, MIN_CYCLE_INTERVAL, MAX_CYCLE_INTERVAL);
}

void ConfigStorage::setRandomOrder(bool random) { 
    config.randomOrder = random; 
}

void ConfigStorage::setCurrentImageIndex(int index) { 
    if (index >= 0 && index < config.imageSourceCount) {
        config.currentImageIndex = index;
    }
}

void ConfigStorage::setImageSourceCount(int count) { 
    config.imageSourceCount = constrain(count, 1, 10);
}

void ConfigStorage::setImageSource(int index, const String& url) {
    if (index >= 0 && index < 10) {
        config.imageSources[index] = url;
    }
}

void ConfigStorage::addImageSource(const String& url) {
    if (config.imageSourceCount < 10) {
        config.imageSources[config.imageSourceCount] = url;
        config.imageSourceCount++;
    }
}

void ConfigStorage::removeImageSource(int index) {
    if (index >= 0 && index < config.imageSourceCount && config.imageSourceCount > 1) {
        // Shift all sources after the removed index
        for (int i = index; i < config.imageSourceCount - 1; i++) {
            config.imageSources[i] = config.imageSources[i + 1];
        }
        config.imageSources[config.imageSourceCount - 1] = "";
        config.imageSourceCount--;
        
        // Adjust current index if necessary
        if (config.currentImageIndex >= config.imageSourceCount) {
            config.currentImageIndex = 0;
        }
    }
}

void ConfigStorage::clearImageSources() {
    for (int i = 0; i < 10; i++) {
        config.imageSources[i] = "";
    }
    config.imageSourceCount = 0;
    config.currentImageIndex = 0;
}

// Multi-image cycling getters
bool ConfigStorage::getCyclingEnabled() { 
    return config.cyclingEnabled; 
}

unsigned long ConfigStorage::getCycleInterval() { 
    return config.cycleInterval; 
}

bool ConfigStorage::getRandomOrder() { 
    return config.randomOrder; 
}

int ConfigStorage::getCurrentImageIndex() { 
    return config.currentImageIndex; 
}

int ConfigStorage::getImageSourceCount() { 
    return config.imageSourceCount; 
}

String ConfigStorage::getImageSource(int index) {
    if (index >= 0 && index < config.imageSourceCount) {
        return config.imageSources[index];
    }
    return "";
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
        if (i > 0) json += ",";
        json += "\"" + config.imageSources[i] + "\"";
    }
    json += "]";
    return json;
}

// Per-image transformation setters
void ConfigStorage::setImageScaleX(int index, float scale) {
    if (index >= 0 && index < 10) {
        config.imageTransforms[index].scaleX = constrain(scale, MIN_SCALE, MAX_SCALE);
    }
}

void ConfigStorage::setImageScaleY(int index, float scale) {
    if (index >= 0 && index < 10) {
        config.imageTransforms[index].scaleY = constrain(scale, MIN_SCALE, MAX_SCALE);
    }
}

void ConfigStorage::setImageOffsetX(int index, int offset) {
    if (index >= 0 && index < 10) {
        config.imageTransforms[index].offsetX = offset;
    }
}

void ConfigStorage::setImageOffsetY(int index, int offset) {
    if (index >= 0 && index < 10) {
        config.imageTransforms[index].offsetY = offset;
    }
}

void ConfigStorage::setImageRotation(int index, float rotation) {
    if (index >= 0 && index < 10) {
        // Normalize rotation to 0, 90, 180, or 270 degrees
        float normalizedRotation = rotation;
        while (normalizedRotation >= 360.0f) normalizedRotation -= 360.0f;
        while (normalizedRotation < 0.0f) normalizedRotation += 360.0f;
        
        // Round to nearest 90 degrees for valid rotation values
        int rotationStep = round(normalizedRotation / 90.0f);
        config.imageTransforms[index].rotation = rotationStep * 90.0f;
    }
}

void ConfigStorage::copyDefaultsToImageTransform(int index) {
    if (index >= 0 && index < 10) {
        config.imageTransforms[index].scaleX = config.defaultScaleX;
        config.imageTransforms[index].scaleY = config.defaultScaleY;
        config.imageTransforms[index].offsetX = config.defaultOffsetX;
        config.imageTransforms[index].offsetY = config.defaultOffsetY;
        config.imageTransforms[index].rotation = config.defaultRotation;
    }
}

void ConfigStorage::copyAllDefaultsToImageTransforms() {
    for (int i = 0; i < 10; i++) {
        copyDefaultsToImageTransform(i);
    }
}

// Per-image transformation getters
float ConfigStorage::getImageScaleX(int index) {
    if (index >= 0 && index < 10) {
        return config.imageTransforms[index].scaleX;
    }
    return DEFAULT_SCALE_X;
}

float ConfigStorage::getImageScaleY(int index) {
    if (index >= 0 && index < 10) {
        return config.imageTransforms[index].scaleY;
    }
    return DEFAULT_SCALE_Y;
}

int ConfigStorage::getImageOffsetX(int index) {
    if (index >= 0 && index < 10) {
        return config.imageTransforms[index].offsetX;
    }
    return DEFAULT_OFFSET_X;
}

int ConfigStorage::getImageOffsetY(int index) {
    if (index >= 0 && index < 10) {
        return config.imageTransforms[index].offsetY;
    }
    return DEFAULT_OFFSET_Y;
}

float ConfigStorage::getImageRotation(int index) {
    if (index >= 0 && index < 10) {
        return config.imageTransforms[index].rotation;
    }
    return DEFAULT_ROTATION;
}

String ConfigStorage::getImageTransformsAsJson() {
    String json = "[";
    for (int i = 0; i < config.imageSourceCount; i++) {
        if (i > 0) json += ",";
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
