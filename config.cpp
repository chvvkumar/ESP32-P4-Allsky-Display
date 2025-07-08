#include "config.h"
#include "config_storage.h"

// =============================================================================
// DYNAMIC CONFIGURATION IMPLEMENTATION
// =============================================================================
// Configuration is now loaded from persistent storage via ConfigStorage
// These variables will be updated from stored values at runtime

// WiFi Configuration - Will be loaded from storage
const char* WIFI_SSID = nullptr;
const char* WIFI_PASSWORD = nullptr;

// MQTT Configuration - Will be loaded from storage
const char* MQTT_SERVER = nullptr;
int MQTT_PORT = 1883;
const char* MQTT_USER = nullptr;
const char* MQTT_PASSWORD = nullptr;
const char* MQTT_CLIENT_ID = nullptr;

// MQTT Topics - Will be loaded from storage
const char* MQTT_REBOOT_TOPIC = nullptr;
const char* MQTT_BRIGHTNESS_TOPIC = nullptr;
const char* MQTT_BRIGHTNESS_STATUS_TOPIC = nullptr;

// Image Configuration - Will be loaded from storage
const char* IMAGE_URL = nullptr;

// Internal string storage for dynamic configuration
String wifiSSIDStr, wifiPasswordStr;
String mqttServerStr, mqttUserStr, mqttPasswordStr, mqttClientIDStr;
String mqttRebootTopicStr, mqttBrightnessTopicStr, mqttBrightnessStatusTopicStr;
String imageURLStr;

// =============================================================================
// CONFIGURATION INITIALIZATION
// =============================================================================

void initializeConfiguration() {
    // Initialize configuration storage
    configStorage.begin();
    
    // Load values from storage and update global variables
    wifiSSIDStr = configStorage.getWiFiSSID();
    wifiPasswordStr = configStorage.getWiFiPassword();
    mqttServerStr = configStorage.getMQTTServer();
    MQTT_PORT = configStorage.getMQTTPort();
    mqttUserStr = configStorage.getMQTTUser();
    mqttPasswordStr = configStorage.getMQTTPassword();
    mqttClientIDStr = configStorage.getMQTTClientID();
    mqttRebootTopicStr = configStorage.getMQTTRebootTopic();
    mqttBrightnessTopicStr = configStorage.getMQTTBrightnessTopic();
    mqttBrightnessStatusTopicStr = configStorage.getMQTTBrightnessStatusTopic();
    imageURLStr = configStorage.getImageURL();
    
    // Update const char* pointers to point to string data
    WIFI_SSID = wifiSSIDStr.c_str();
    WIFI_PASSWORD = wifiPasswordStr.c_str();
    MQTT_SERVER = mqttServerStr.c_str();
    MQTT_USER = mqttUserStr.c_str();
    MQTT_PASSWORD = mqttPasswordStr.c_str();
    MQTT_CLIENT_ID = mqttClientIDStr.c_str();
    MQTT_REBOOT_TOPIC = mqttRebootTopicStr.c_str();
    MQTT_BRIGHTNESS_TOPIC = mqttBrightnessTopicStr.c_str();
    MQTT_BRIGHTNESS_STATUS_TOPIC = mqttBrightnessStatusTopicStr.c_str();
    IMAGE_URL = imageURLStr.c_str();
    
    Serial.println("Configuration loaded from persistent storage");
    if (configStorage.hasStoredConfig()) {
        Serial.println("Using stored configuration");
    } else {
        Serial.println("Using default configuration (first boot)");
    }
}

void reloadConfiguration() {
    // Reload configuration from storage (useful after web config changes)
    initializeConfiguration();
    Serial.println("Configuration reloaded from storage");
}
