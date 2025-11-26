#pragma once
#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

// Configuration storage class for persistent settings
class ConfigStorage {
public:
    // Constructor
    ConfigStorage();
    
    // Initialize storage system
    bool begin();
    
    // Load configuration from NVS storage
    void loadConfig();
    
    // Save configuration to NVS storage
    void saveConfig();
    
    // Reset to factory defaults
    void resetToDefaults();
    
    // Individual parameter setters
    void setWiFiSSID(const String& ssid);
    void setWiFiPassword(const String& password);
    void setMQTTServer(const String& server);
    void setMQTTPort(int port);
    void setMQTTUser(const String& user);
    void setMQTTPassword(const String& password);
    void setMQTTClientID(const String& clientId);
    void setImageURL(const String& url);
    
    // Home Assistant Discovery setters
    void setHADiscoveryEnabled(bool enabled);
    void setHADeviceName(const String& name);
    void setHADiscoveryPrefix(const String& prefix);
    void setHAStateTopic(const String& topic);
    void setHASensorUpdateInterval(unsigned long interval);
    void setDefaultBrightness(int brightness);
    void setBrightnessAutoMode(bool autoMode);
    void setUpdateInterval(unsigned long interval);
    void setMQTTReconnectInterval(unsigned long interval);
    void setDefaultScaleX(float scale);
    void setDefaultScaleY(float scale);
    void setDefaultOffsetX(int offset);
    void setDefaultOffsetY(int offset);
    void setDefaultRotation(float rotation);
    void setBacklightFreq(int freq);
    void setBacklightResolution(int resolution);
    void setWatchdogTimeout(unsigned long timeout);
    void setCriticalHeapThreshold(size_t threshold);
    void setCriticalPSRAMThreshold(size_t threshold);
    
    // Multi-image cycling setters
    void setCyclingEnabled(bool enabled);
    void setCycleInterval(unsigned long interval);
    void setRandomOrder(bool random);
    void setCurrentImageIndex(int index);
    void setImageSourceCount(int count);
    void setImageSource(int index, const String& url);
    void addImageSource(const String& url);
    void removeImageSource(int index);
    void clearImageSources();
    
    // Individual parameter getters
    String getWiFiSSID();
    String getWiFiPassword();
    String getMQTTServer();
    int getMQTTPort();
    String getMQTTUser();
    String getMQTTPassword();
    String getMQTTClientID();
    String getImageURL();
    
    // Home Assistant Discovery getters
    bool getHADiscoveryEnabled();
    String getHADeviceName();
    String getHADiscoveryPrefix();
    String getHAStateTopic();
    unsigned long getHASensorUpdateInterval();
    int getDefaultBrightness();
    bool getBrightnessAutoMode();
    unsigned long getUpdateInterval();
    unsigned long getMQTTReconnectInterval();
    float getDefaultScaleX();
    float getDefaultScaleY();
    int getDefaultOffsetX();
    int getDefaultOffsetY();
    float getDefaultRotation();
    int getBacklightFreq();
    int getBacklightResolution();
    unsigned long getWatchdogTimeout();
    size_t getCriticalHeapThreshold();
    size_t getCriticalPSRAMThreshold();
    
    // Multi-image cycling getters
    bool getCyclingEnabled();
    unsigned long getCycleInterval();
    bool getRandomOrder();
    int getCurrentImageIndex();
    int getImageSourceCount();
    String getImageSource(int index);
    String getCurrentImageURL();
    String getAllImageSources(); // Returns JSON-formatted string of all sources
    
    // Per-image transformation setters
    void setImageScaleX(int index, float scale);
    void setImageScaleY(int index, float scale);
    void setImageOffsetX(int index, int offset);
    void setImageOffsetY(int index, int offset);
    void setImageRotation(int index, float rotation);
    void copyDefaultsToImageTransform(int index);
    void copyAllDefaultsToImageTransforms();
    
    // Per-image transformation getters
    float getImageScaleX(int index);
    float getImageScaleY(int index);
    int getImageOffsetX(int index);
    int getImageOffsetY(int index);
    float getImageRotation(int index);
    String getImageTransformsAsJson(); // Get all transforms as JSON string
    
    // Check if configuration exists
    bool hasStoredConfig();

private:
    Preferences preferences;
    static const char* NAMESPACE;
    
    // Image transformation settings structure
    struct ImageTransform {
        float scaleX;
        float scaleY;
        int offsetX;
        int offsetY;
        float rotation;
    };
    
    // Configuration structure
    struct Config {
        // Network settings
        String wifiSSID;
        String wifiPassword;
        
        // MQTT settings
        String mqttServer;
        int mqttPort;
        String mqttUser;
        String mqttPassword;
        String mqttClientID;
        
        // Home Assistant Discovery settings
        bool haDiscoveryEnabled;
        String haDeviceName;
        String haDiscoveryPrefix;
        String haStateTopic;
        unsigned long haSensorUpdateInterval;
        
        // Image settings
        String imageURL;  // Legacy single image URL
        
        // Multi-image cycling settings
        bool cyclingEnabled;
        unsigned long cycleInterval;
        bool randomOrder;
        int currentImageIndex;
        int imageSourceCount;
        String imageSources[10];  // Array of image source URLs (MAX_IMAGE_SOURCES)
        
        // Per-image transformation settings
        ImageTransform imageTransforms[10];  // Transformation settings for each image source
        
        // Display settings
        int defaultBrightness;
        bool brightnessAutoMode;
        float defaultScaleX;
        float defaultScaleY;
        int defaultOffsetX;
        int defaultOffsetY;
        float defaultRotation;
        int backlightFreq;
        int backlightResolution;
        
        // Advanced settings
        unsigned long updateInterval;
        unsigned long mqttReconnectInterval;
        unsigned long watchdogTimeout;
        size_t criticalHeapThreshold;
        size_t criticalPSRAMThreshold;
    } config;
    
    void setDefaults();
};

// Global instance
extern ConfigStorage configStorage;

#endif // CONFIG_STORAGE_H
