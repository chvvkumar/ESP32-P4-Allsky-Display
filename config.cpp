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

// Image Configuration - Will be loaded from storage
const char* IMAGE_URL = nullptr;

// =============================================================================
// DEFAULT IMAGE SOURCES CONFIGURATION
// =============================================================================
// Configure your default image sources here. These will be loaded on first boot
// or when the configuration is reset to defaults.

const char* DEFAULT_IMAGE_SOURCES[] = {
    "http://allskypi5.lan/current/resized/image.jpg"      // Default source 1
    // Add more image URLs here as needed (up to MAX_IMAGE_SOURCES = 10)
    // "https://your-server.com/camera/image4.jpg",
    // "https://your-server.com/camera/image5.jpg",
};

const int DEFAULT_IMAGE_SOURCE_COUNT = sizeof(DEFAULT_IMAGE_SOURCES) / sizeof(DEFAULT_IMAGE_SOURCES[0]);
const bool DEFAULT_CYCLING_ENABLED = true;     // Enable cycling by default
const bool DEFAULT_RANDOM_ORDER = false;       // Use sequential order by default

// Internal string storage for dynamic configuration
String wifiSSIDStr, wifiPasswordStr;
String mqttServerStr, mqttUserStr, mqttPasswordStr, mqttClientIDStr;
String imageURLStr;

// =============================================================================
// CONFIGURATION INITIALIZATION
// =============================================================================

void initializeConfiguration() {
    // Initialize configuration storage
    configStorage.begin();
    
    // Check if this is first boot or configuration needs initialization
    if (!configStorage.hasStoredConfig()) {
        Serial.println("First boot detected - initializing with default image sources");
        
        // Clear any existing image sources
        configStorage.clearImageSources();
        
        // Load default image sources from config
        for (int i = 0; i < DEFAULT_IMAGE_SOURCE_COUNT; i++) {
            configStorage.addImageSource(String(DEFAULT_IMAGE_SOURCES[i]));
            Serial.printf("Added default image source %d: %s\n", i+1, DEFAULT_IMAGE_SOURCES[i]);
        }
        
        // Set default cycling configuration
        configStorage.setCyclingEnabled(DEFAULT_CYCLING_ENABLED);
        configStorage.setRandomOrder(DEFAULT_RANDOM_ORDER);
        configStorage.setCycleInterval(DEFAULT_CYCLE_INTERVAL);
        
        // Save the default configuration
        configStorage.saveConfig();
        
        Serial.printf("Initialized %d default image sources with cycling %s\n", 
                     DEFAULT_IMAGE_SOURCE_COUNT,
                     DEFAULT_CYCLING_ENABLED ? "enabled" : "disabled");
    }
    
    // Load values from storage and update global variables
    wifiSSIDStr = configStorage.getWiFiSSID();
    wifiPasswordStr = configStorage.getWiFiPassword();
    mqttServerStr = configStorage.getMQTTServer();
    MQTT_PORT = configStorage.getMQTTPort();
    mqttUserStr = configStorage.getMQTTUser();
    mqttPasswordStr = configStorage.getMQTTPassword();
    mqttClientIDStr = configStorage.getMQTTClientID();
    imageURLStr = configStorage.getImageURL();
    
    // Update const char* pointers to point to string data
    WIFI_SSID = wifiSSIDStr.c_str();
    WIFI_PASSWORD = wifiPasswordStr.c_str();
    MQTT_SERVER = mqttServerStr.c_str();
    MQTT_USER = mqttUserStr.c_str();
    MQTT_PASSWORD = mqttPasswordStr.c_str();
    MQTT_CLIENT_ID = mqttClientIDStr.c_str();
    IMAGE_URL = imageURLStr.c_str();
    
    Serial.println("Configuration loaded from persistent storage");
    if (configStorage.hasStoredConfig()) {
        Serial.println("Using stored configuration");
    } else {
        Serial.println("Using default configuration (first boot)");
    }
    
    // Print current image source configuration
    int sourceCount = configStorage.getImageSourceCount();
    Serial.printf("Current image sources: %d configured\n", sourceCount);
    for (int i = 0; i < sourceCount; i++) {
        Serial.printf("  [%d] %s\n", i+1, configStorage.getImageSource(i).c_str());
    }
    Serial.printf("Cycling: %s, Random: %s, Interval: %lu ms\n",
                 configStorage.getCyclingEnabled() ? "enabled" : "disabled",
                 configStorage.getRandomOrder() ? "enabled" : "disabled",
                 configStorage.getCycleInterval());
}

void reloadConfiguration() {
    // Reload configuration from storage (useful after web config changes)
    initializeConfiguration();
    Serial.println("Configuration reloaded from storage");
}
