#pragma once
#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Dirty field bitmask constants for per-group NVS write tracking
// Each bit represents a group of related config fields
static const uint32_t DIRTY_WIFI        = 0x00000001;  // WiFi SSID, password, provisioned
static const uint32_t DIRTY_MQTT        = 0x00000002;  // MQTT server, port, user, password, client ID
static const uint32_t DIRTY_HA_DISC     = 0x00000004;  // HA Discovery enabled, name, prefix, topic, interval
static const uint32_t DIRTY_IMAGE       = 0x00000008;  // Image URL, sources array, source count, enabled, durations
static const uint32_t DIRTY_CYCLING     = 0x00000010;  // Cycling enabled, mode, interval, random, current index
static const uint32_t DIRTY_DISPLAY     = 0x00000020;  // Brightness, backlight, display type, color temp
static const uint32_t DIRTY_ADVANCED    = 0x00000040;  // Update interval, MQTT reconnect, watchdog, heap/PSRAM thresholds
static const uint32_t DIRTY_TIME        = 0x00000080;  // NTP server, timezone, NTP enabled
static const uint32_t DIRTY_HA_REST     = 0x00000100;  // HA REST base URL, token, sensor entity, lux, brightness, poll
static const uint32_t DIRTY_TRANSFORMS  = 0x00000200;  // Per-image and default scale, offset, rotation
static const uint32_t DIRTY_DEVICE      = 0x00000400;  // Device name
static const uint32_t DIRTY_LOGGING     = 0x00000800;  // Log severity
static const uint32_t DIRTY_ALL         = 0xFFFFFFFF;  // All fields dirty (used for resetToDefaults)

// RAII lock guard for ConfigStorage mutex
class ConfigLock {
public:
    explicit ConfigLock(SemaphoreHandle_t mutex) : _mutex(mutex), _acquired(false) {
        if (_mutex != nullptr) {
            _acquired = (xSemaphoreTake(_mutex, pdMS_TO_TICKS(1000)) == pdTRUE);
            if (!_acquired) {
                Serial.println("WARNING: ConfigStorage mutex acquisition timed out!");
            }
        }
    }
    ~ConfigLock() {
        if (_acquired && _mutex != nullptr) {
            xSemaphoreGive(_mutex);
        }
    }
    bool acquired() const { return _acquired; }

    // Non-copyable
    ConfigLock(const ConfigLock&) = delete;
    ConfigLock& operator=(const ConfigLock&) = delete;
private:
    SemaphoreHandle_t _mutex;
    bool _acquired;
};

// Configuration storage class for persistent settings
class ConfigStorage {
public:
    // Constructor
    ConfigStorage();

    // Initialize storage system
    bool begin();

    // Load configuration from NVS storage
    void loadConfig();

    // Save configuration to NVS storage (only writes dirty field groups)
    void saveConfig();

    // Reset to factory defaults
    void resetToDefaults();

    // Individual parameter setters
    void setDeviceName(const String& name);
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
    void setDefaultImageDuration(unsigned long duration);
    void setBacklightFreq(int freq);
    void setBacklightResolution(int resolution);
    void setWatchdogTimeout(unsigned long timeout);
    void setCriticalHeapThreshold(size_t threshold);
    void setCriticalPSRAMThreshold(size_t threshold);

    // Multi-image cycling setters
    void setCyclingEnabled(bool enabled);
    void setImageUpdateMode(int mode);
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
    int getEnabledImageCount();
    void setImageDuration(int index, unsigned long duration);
    unsigned long getImageDuration(int index);

    // Individual parameter getters
    String getDeviceName();
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
    unsigned long getDefaultImageDuration();
    int getBacklightFreq();
    int getBacklightResolution();
    unsigned long getWatchdogTimeout();
    size_t getCriticalHeapThreshold();
    size_t getCriticalPSRAMThreshold();

    // Multi-image cycling getters
    bool getCyclingEnabled();
    int getImageUpdateMode();
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

    // Display hardware setters
    void setDisplayType(int type);

    // Display hardware getters
    int getDisplayType();

    // Color temperature setters/getters
    void setColorTemp(int temp);
    int getColorTemp();

private:
    Preferences preferences;
    static const char* NAMESPACE;

    // FreeRTOS mutex for thread-safe access
    SemaphoreHandle_t _mutex;

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
        // Device settings
        String deviceName;

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
        int imageUpdateMode;  // 0=automatic cycling, 1=API-triggered refresh
        unsigned long cycleInterval;
        bool randomOrder;
        int currentImageIndex;
        int imageSourceCount;
        String imageSources[10];  // Array of image source URLs (MAX_IMAGE_SOURCES)
        bool imageEnabled[10];  // Array of enabled/disabled states for each image source
        unsigned long imageDurations[10];  // Display duration in seconds for each image source

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
        unsigned long defaultImageDuration;
        int backlightFreq;
        int backlightResolution;

        // Display hardware
        int displayType;  // 1=3.4" DSI, 2=4.0" DSI

        // Color temperature
        int colorTemp;  // Display color temperature in Kelvin (2000-10000)

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

    // Helper to mark a field group dirty
    void markDirty(uint32_t fields);

    // Dirty flag to track if configuration has changed
    bool _dirty = false;

    // Per-field-group dirty bitmask for selective NVS writes
    uint32_t _dirtyFields = 0;
};

// Global instance
extern ConfigStorage configStorage;

#endif // CONFIG_STORAGE_H
