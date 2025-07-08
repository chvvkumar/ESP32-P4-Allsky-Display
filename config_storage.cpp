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
    config.wifiSSID = "";
    config.wifiPassword = "";
    
    config.mqttServer = "192.168.1.250";
    config.mqttPort = 1883;
    config.mqttUser = "";
    config.mqttPassword = "";
    config.mqttClientID = "ESP32_Allsky_Display";
    config.mqttRebootTopic = "Astro/AllSky/display/reboot";
    config.mqttBrightnessTopic = "Astro/AllSky/display/brightness";
    config.mqttBrightnessStatusTopic = "Astro/AllSky/display/brightness/status";
    
    config.imageURL = "";
    
    config.defaultBrightness = DEFAULT_BRIGHTNESS;
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
        config.wifiSSID = preferences.getString("wifi_ssid", config.wifiSSID);
        config.wifiPassword = preferences.getString("wifi_pwd", config.wifiPassword);
        
        config.mqttServer = preferences.getString("mqtt_server", config.mqttServer);
        config.mqttPort = preferences.getInt("mqtt_port", config.mqttPort);
        config.mqttUser = preferences.getString("mqtt_user", config.mqttUser);
        config.mqttPassword = preferences.getString("mqtt_pwd", config.mqttPassword);
        config.mqttClientID = preferences.getString("mqtt_client", config.mqttClientID);
        config.mqttRebootTopic = preferences.getString("mqtt_reboot", config.mqttRebootTopic);
        config.mqttBrightnessTopic = preferences.getString("mqtt_bright", config.mqttBrightnessTopic);
        config.mqttBrightnessStatusTopic = preferences.getString("mqtt_b_stat", config.mqttBrightnessStatusTopic);
        
        config.imageURL = preferences.getString("image_url", config.imageURL);
        
        config.defaultBrightness = preferences.getInt("def_bright", config.defaultBrightness);
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
    
    preferences.putString("wifi_ssid", config.wifiSSID);
    preferences.putString("wifi_pwd", config.wifiPassword);
    
    preferences.putString("mqtt_server", config.mqttServer);
    preferences.putInt("mqtt_port", config.mqttPort);
    preferences.putString("mqtt_user", config.mqttUser);
    preferences.putString("mqtt_pwd", config.mqttPassword);
    preferences.putString("mqtt_client", config.mqttClientID);
    preferences.putString("mqtt_reboot", config.mqttRebootTopic);
    preferences.putString("mqtt_bright", config.mqttBrightnessTopic);
    preferences.putString("mqtt_b_stat", config.mqttBrightnessStatusTopic);
    
    preferences.putString("image_url", config.imageURL);
    
    preferences.putInt("def_bright", config.defaultBrightness);
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

// Setters
void ConfigStorage::setWiFiSSID(const String& ssid) { config.wifiSSID = ssid; }
void ConfigStorage::setWiFiPassword(const String& password) { config.wifiPassword = password; }
void ConfigStorage::setMQTTServer(const String& server) { config.mqttServer = server; }
void ConfigStorage::setMQTTPort(int port) { config.mqttPort = port; }
void ConfigStorage::setMQTTUser(const String& user) { config.mqttUser = user; }
void ConfigStorage::setMQTTPassword(const String& password) { config.mqttPassword = password; }
void ConfigStorage::setMQTTClientID(const String& clientId) { config.mqttClientID = clientId; }
void ConfigStorage::setMQTTRebootTopic(const String& topic) { config.mqttRebootTopic = topic; }
void ConfigStorage::setMQTTBrightnessTopic(const String& topic) { config.mqttBrightnessTopic = topic; }
void ConfigStorage::setMQTTBrightnessStatusTopic(const String& topic) { config.mqttBrightnessStatusTopic = topic; }
void ConfigStorage::setImageURL(const String& url) { config.imageURL = url; }
void ConfigStorage::setDefaultBrightness(int brightness) { config.defaultBrightness = brightness; }
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
String ConfigStorage::getMQTTRebootTopic() { return config.mqttRebootTopic; }
String ConfigStorage::getMQTTBrightnessTopic() { return config.mqttBrightnessTopic; }
String ConfigStorage::getMQTTBrightnessStatusTopic() { return config.mqttBrightnessStatusTopic; }
String ConfigStorage::getImageURL() { return config.imageURL; }
int ConfigStorage::getDefaultBrightness() { return config.defaultBrightness; }
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
