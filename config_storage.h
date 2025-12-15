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
    bool removeImageSource(int index);
    void clearImageSources();
    void setImageEnabled(int index, bool enabled);
    bool isImageEnabled(int index);
    
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
    
    // WiFi provisioning status
    bool isWiFiProvisioned();
    void setWiFiProvisioned(bool provisioned);
    
    // Log severity filtering
    void setMinLogSeverity(int severity);
    int getMinLogSeverity();
    
    // Time settings
    void setNTPServer(const String& server);
    String getNTPServer();
    void setTimezone(const String& tz);
    String getTimezone();
    void setNTPEnabled(bool enabled);
    bool getNTPEnabled();
    
    // Home Assistant REST Control setters
    void setHABaseUrl(const String& url);
    void setHAAccessToken(const String& token);
    void setHALightSensorEntity(const String& entity);
    void setLightSensorMinLux(float minLux);
    void setLightSensorMaxLux(float maxLux);
    void setDisplayMinBrightness(int minBrightness);
    void setDisplayMaxBrightness(int maxBrightness);
    void setUseHARestControl(bool enabled);
    void setHAPollInterval(unsigned long interval);
    void setLightSensorMappingMode(int mode);
    
    // Home Assistant REST Control getters
    String getHABaseUrl();
    String getHAAccessToken();
    String getHALightSensorEntity();
    float getLightSensorMinLux();
    float getLightSensorMaxLux();
    int getDisplayMinBrightness();
    int getDisplayMaxBrightness();
    bool getUseHARestControl();
    unsigned long getHAPollInterval();
    int getLightSensorMappingMode();

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
        bool wifiProvisioned;
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
        bool imageEnabled[10];  // Array of enabled/disabled states for each image source
        
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
        
        // Logging settings
        int minLogSeverity;  // Minimum severity level for WebSocket console
        
        // Time settings
        String ntpServer;
        String timezone;  // POSIX timezone string
        bool ntpEnabled;
        
        // Home Assistant REST Control settings
        String haBaseUrl;
        String haAccessToken;
        String haLightSensorEntity;
        float lightSensorMinLux;
        float lightSensorMaxLux;
        int displayMinBrightness;
        int displayMaxBrightness;
        bool useHaRestControl;
        unsigned long haPollInterval;
        int lightSensorMappingMode;  // 0=Linear, 1=Logarithmic, 2=Threshold
    } config;
    
    void setDefaults();
    
    // Dirty flag to track if configuration has changed
    bool _dirty = false;
};

// Global instance
extern ConfigStorage configStorage;

#endif // CONFIG_STORAGE_H
