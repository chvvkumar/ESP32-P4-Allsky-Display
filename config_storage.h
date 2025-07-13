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
    void setMQTTRebootTopic(const String& topic);
    void setMQTTBrightnessTopic(const String& topic);
    void setMQTTBrightnessStatusTopic(const String& topic);
    void setImageURL(const String& url);
    void setDefaultBrightness(int brightness);
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
    String getMQTTRebootTopic();
    String getMQTTBrightnessTopic();
    String getMQTTBrightnessStatusTopic();
    String getImageURL();
    int getDefaultBrightness();
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
    
    // Check if configuration exists
    bool hasStoredConfig();

private:
    Preferences preferences;
    static const char* NAMESPACE;
    
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
        String mqttRebootTopic;
        String mqttBrightnessTopic;
        String mqttBrightnessStatusTopic;
        
        // Image settings
        String imageURL;  // Legacy single image URL
        
        // Multi-image cycling settings
        bool cyclingEnabled;
        unsigned long cycleInterval;
        bool randomOrder;
        int currentImageIndex;
        int imageSourceCount;
        String imageSources[10];  // Array of image source URLs (MAX_IMAGE_SOURCES)
        
        // Display settings
        int defaultBrightness;
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
