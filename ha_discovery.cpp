#include "ha_discovery.h"
#include "display_manager.h"
#include "system_monitor.h"
#include "crash_logger.h"
#include "logging.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Global instances
HADiscovery haDiscovery;
extern CrashLogger crashLogger;

HADiscovery::HADiscovery() :
    mqttClient(nullptr),
    lastSensorUpdate(0),
    lastSensorPublish(0),
    _discoveryStep(0),
    _discoveryInProgress(false),
    _lastDiscoveryPublish(0),
    _discoveryFailed(false)
{
}

void HADiscovery::begin(PubSubClient* client) {
    LOG_INFO("[HA] Initializing Home Assistant discovery");
    mqttClient = client;
    deviceId = getDeviceId();
    baseTopic = getBaseTopic();
    
    // Pre-build cached topic strings to prevent memory leaks
    cachedAvailabilityTopic = baseTopic + "/availability";
    cachedCommandTopicFilter = baseTopic + "/+/set";
    cachedAttributesTopic = baseTopic + "/attributes";
    
    LOG_DEBUG_F("[HA] Device ID: %s\n", deviceId.c_str());
    LOG_DEBUG_F("[HA] Base topic: %s\n", baseTopic.c_str());
}

String HADiscovery::getDeviceId() {
    // Use MAC address as unique device ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
}

String HADiscovery::getBaseTopic() {
    return configStorage.getHAStateTopic() + "/" + deviceId;
}

String HADiscovery::buildDiscoveryTopic(const char* component, const char* entityId) {
    return configStorage.getHADiscoveryPrefix() + "/" + String(component) + "/" + 
           deviceId + "/" + String(entityId) + "/config";
}

String HADiscovery::buildStateTopic(const char* entity) {
    if (entity == nullptr) {
        return baseTopic + "/state";
    }
    return baseTopic + "/" + String(entity) + "/state";
}

String HADiscovery::buildCommandTopic(const char* entity) {
    return baseTopic + "/" + String(entity) + "/set";
}

String HADiscovery::buildAttributesTopic() {
    return cachedAttributesTopic;
}

String HADiscovery::getAvailabilityTopic() {
    return cachedAvailabilityTopic;
}

String HADiscovery::getCommandTopicFilter() {
    return cachedCommandTopicFilter;
}

String HADiscovery::getDeviceJson() {
    String json = "{";
    json += "\"identifiers\":[\"" + deviceId + "\"],";
    json += "\"name\":\"" + configStorage.getDeviceName() + "\",";
    json += "\"model\":\"ESP32-P4-WIFI6-Touch-LCD\",";
    json += "\"manufacturer\":\"chvvkumar\",";
    json += "\"sw_version\":\"1.0\",";
    json += "\"configuration_url\":\"http://" + WiFi.localIP().toString() + ":8080\"";
    json += "}";
    return json;
}

bool HADiscovery::publishLightDiscovery() {
    LOG_DEBUG("[HA] Publishing light entity discovery");
    String topic = buildDiscoveryTopic("light", "brightness");
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " Brightness\",";
    payload += "\"unique_id\":\"" + deviceId + "_brightness\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"state_topic\":\"" + buildStateTopic("brightness") + "\",";
    payload += "\"command_topic\":\"" + buildCommandTopic("brightness") + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    payload += "\"brightness_scale\":100,";
    payload += "\"brightness_state_topic\":\"" + buildStateTopic("brightness") + "\",";
    payload += "\"brightness_command_topic\":\"" + buildCommandTopic("brightness") + "\",";
    payload += "\"on_command_type\":\"brightness\",";
    payload += "\"icon\":\"mdi:brightness-6\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        LOG_ERROR("[HA] Failed to publish light discovery!");
    }
    return result;
}

bool HADiscovery::publishSwitchDiscovery(const char* entityId, const char* name, const char* icon) {
    String topic = buildDiscoveryTopic("switch", entityId);
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " " + String(name) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_" + String(entityId) + "\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"state_topic\":\"" + buildStateTopic(entityId) + "\",";
    payload += "\"command_topic\":\"" + buildCommandTopic(entityId) + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    payload += "\"payload_on\":\"ON\",";
    payload += "\"payload_off\":\"OFF\",";
    payload += "\"icon\":\"" + String(icon) + "\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    return result;
}

bool HADiscovery::publishNumberDiscovery(const char* entityId, const char* name, 
                                         float min, float max, float step, 
                                         const char* unit, const char* icon) {
    String topic = buildDiscoveryTopic("number", entityId);
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " " + String(name) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_" + String(entityId) + "\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"state_topic\":\"" + buildStateTopic(entityId) + "\",";
    payload += "\"command_topic\":\"" + buildCommandTopic(entityId) + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    payload += "\"min\":" + String(min, 2) + ",";
    payload += "\"max\":" + String(max, 2) + ",";
    payload += "\"step\":" + String(step, 2) + ",";
    if (unit != nullptr && strlen(unit) > 0) {
        payload += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    }
    payload += "\"icon\":\"" + String(icon) + "\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    return result;
}

bool HADiscovery::publishSelectDiscovery() {
    String topic = buildDiscoveryTopic("select", "image_source");
    
    // Build options array from configured image sources
    String options = "[";
    int imageCount = configStorage.getImageSourceCount();
    for (int i = 0; i < imageCount; i++) {
        if (i > 0) options += ",";
        options += "\"Image " + String(i + 1) + "\"";
    }
    options += "]";
    
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " Image Source\",";
    payload += "\"unique_id\":\"" + deviceId + "_image_source\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"state_topic\":\"" + buildStateTopic("image_source") + "\",";
    payload += "\"command_topic\":\"" + buildCommandTopic("image_source") + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    payload += "\"options\":" + options + ",";
    payload += "\"icon\":\"mdi:image-multiple\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    return result;
}

bool HADiscovery::publishButtonDiscovery(const char* entityId, const char* name, const char* icon) {
    String topic = buildDiscoveryTopic("button", entityId);
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " " + String(name) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_" + String(entityId) + "\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"command_topic\":\"" + buildCommandTopic(entityId) + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    payload += "\"payload_press\":\"PRESS\",";
    payload += "\"icon\":\"" + String(icon) + "\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    return result;
}

bool HADiscovery::publishSensorDiscovery(const char* entityId, const char* name,
                                         const char* unit, const char* deviceClass,
                                         const char* icon) {
    String topic = buildDiscoveryTopic("sensor", entityId);
    String payload = "{";
    payload += "\"name\":\"" + configStorage.getDeviceName() + " " + String(name) + "\",";
    payload += "\"unique_id\":\"" + deviceId + "_" + String(entityId) + "\",";
    payload += "\"device\":" + getDeviceJson() + ",";
    payload += "\"state_topic\":\"" + buildStateTopic(entityId) + "\",";
    payload += "\"availability_topic\":\"" + getAvailabilityTopic() + "\",";
    if (unit != nullptr && strlen(unit) > 0) {
        payload += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    }
    if (deviceClass != nullptr && strlen(deviceClass) > 0) {
        payload += "\"device_class\":\"" + String(deviceClass) + "\",";
    }
    payload += "\"icon\":\"" + String(icon) + "\"";
    payload += "}";
    
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    return result;
}

bool HADiscovery::startDiscovery() {
    if (!mqttClient || !mqttClient->connected()) {
        LOG_WARNING("[HA] Cannot start discovery - MQTT not connected");
        return false;
    }

    if (!configStorage.getHADiscoveryEnabled()) {
        LOG_DEBUG("[HA] Discovery disabled in configuration");
        return false;
    }

    LOG_INFO("[HA] Starting non-blocking Home Assistant discovery");
    _discoveryStep = 0;
    _discoveryInProgress = true;
    _discoveryFailed = false;
    _lastDiscoveryPublish = 0;  // Allow immediate first publish
    return true;
}

bool HADiscovery::publishDiscoveryStep(int step) {
    switch (step) {
        // Light entity
        case 0:  return publishLightDiscovery();
        // Switch entities
        case 1:  return publishSwitchDiscovery("cycling", "Cycling Enabled", "mdi:image-multiple");
        case 2:  return publishSwitchDiscovery("random_order", "Random Order", "mdi:shuffle");
        case 3:  return publishSwitchDiscovery("auto_brightness", "Auto Brightness", "mdi:brightness-auto");
        // Number entities
        case 4:  return publishNumberDiscovery("cycle_interval", "Cycle Interval", 10, 3600, 10, "s", "mdi:timer");
        case 5:  return publishNumberDiscovery("update_interval", "Update Interval", 10, 3600, 10, "s", "mdi:update");
        // Select entity
        case 6:  return publishSelectDiscovery();
        // Button entities
        case 7:  return publishButtonDiscovery("reboot", "Reboot", "mdi:restart");
        case 8:  return publishButtonDiscovery("next_image", "Next Image", "mdi:skip-next");
        case 9:  return publishButtonDiscovery("reset_transforms", "Reset Transforms", "mdi:restore");
        // Sensor entities
        case 10: return publishSensorDiscovery("current_image", "Current Image URL", "", "", "mdi:image");
        case 11: return publishSensorDiscovery("free_heap", "Free Heap", "KB", "", "mdi:memory");
        case 12: return publishSensorDiscovery("free_psram", "Free PSRAM", "KB", "", "mdi:memory");
        case 13: return publishSensorDiscovery("wifi_rssi", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi");
        case 14: return publishSensorDiscovery("uptime", "Uptime", "s", "duration", "mdi:clock-outline");
        case 15: return publishSensorDiscovery("uptime_readable", "Uptime Readable", "", "", "mdi:clock-outline");
        case 16: return publishSensorDiscovery("image_count", "Image Count", "", "", "mdi:counter");
        case 17: return publishSensorDiscovery("current_image_index", "Current Image Index", "", "", "mdi:numeric");
        case 18: return publishSensorDiscovery("cycling_mode", "Cycling Mode", "", "", "mdi:sync");
        case 19: return publishSensorDiscovery("random_order_status", "Random Order", "", "", "mdi:shuffle-variant");
        case 20: return publishSensorDiscovery("cycle_interval_status", "Cycle Interval", "s", "", "mdi:timer-outline");
        case 21: return publishSensorDiscovery("update_interval_status", "Update Interval", "s", "", "mdi:update");
        case 22: return publishSensorDiscovery("display_width", "Display Width", "px", "", "mdi:monitor-screenshot");
        case 23: return publishSensorDiscovery("display_height", "Display Height", "px", "", "mdi:monitor-screenshot");
        case 24: return publishSensorDiscovery("auto_brightness_status", "Auto Brightness", "", "", "mdi:brightness-auto");
        case 25: return publishSensorDiscovery("brightness_level", "Brightness Level", "%", "", "mdi:brightness-6");
        case 26: return publishSensorDiscovery("temperature_celsius", "Temperature", "\xC2\xB0" "C", "temperature", "mdi:thermometer");
        case 27: return publishSensorDiscovery("temperature_fahrenheit", "Temperature (F)", "\xC2\xB0" "F", "temperature", "mdi:thermometer");
        default: return true;  // Unknown step, skip
    }
}

bool HADiscovery::publishAvailability(bool online) {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    const char* payload = online ? "online" : "offline";
    LOG_DEBUG_F("[HA] Publishing availability: %s\n", payload);
    return mqttClient->publish(cachedAvailabilityTopic.c_str(), payload, true);
}

bool HADiscovery::publishState() {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    if (!configStorage.getHADiscoveryEnabled()) {
        return false;
    }
    
    LOG_DEBUG("[HA] Publishing entity states to Home Assistant");
    
    // Use char buffers to avoid String allocation overhead
    char topic[128];
    char value[32];
    
    // Brightness
    snprintf(topic, sizeof(topic), "%s/brightness/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", displayManager.getBrightness());
    mqttClient->publish(topic, value);
    
    // Cycling
    snprintf(topic, sizeof(topic), "%s/cycling/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getCyclingEnabled() ? "ON" : "OFF");
    
    // Random order
    snprintf(topic, sizeof(topic), "%s/random_order/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getRandomOrder() ? "ON" : "OFF");
    
    // Auto brightness
    snprintf(topic, sizeof(topic), "%s/auto_brightness/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getBrightnessAutoMode() ? "ON" : "OFF");
    
    // Cycle interval (convert milliseconds to seconds)
    snprintf(topic, sizeof(topic), "%s/cycle_interval/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%lu", configStorage.getCycleInterval() / 1000);
    mqttClient->publish(topic, value);
    
    // Update interval (convert milliseconds to seconds)
    snprintf(topic, sizeof(topic), "%s/update_interval/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%lu", configStorage.getUpdateInterval() / 1000);
    mqttClient->publish(topic, value);
    
    // Image source
    snprintf(topic, sizeof(topic), "%s/image_source/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "Image %d", configStorage.getCurrentImageIndex() + 1);
    mqttClient->publish(topic, value);
    
    // Publish sensors
    publishSensors();
    
    return true;
}

bool HADiscovery::publishSensors() {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }

    if (!configStorage.getHADiscoveryEnabled()) {
        return false;
    }

    // Update last publish time
    lastSensorPublish = millis();

    // Reusable buffers to avoid String fragmentation
    char topic[128];
    char value[64];

    // Current image URL (needs String for the URL, but we avoid extra copies)
    snprintf(topic, sizeof(topic), "%s/current_image/state", baseTopic.c_str());
    String currentImageURL = configStorage.getCurrentImageURL();
    mqttClient->publish(topic, currentImageURL.c_str());

    // Free heap (in KB)
    snprintf(topic, sizeof(topic), "%s/free_heap/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%u", (unsigned)(ESP.getFreeHeap() / 1024));
    mqttClient->publish(topic, value);

    // Free PSRAM (in KB)
    snprintf(topic, sizeof(topic), "%s/free_psram/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%u", (unsigned)(ESP.getFreePsram() / 1024));
    mqttClient->publish(topic, value);

    // WiFi RSSI
    snprintf(topic, sizeof(topic), "%s/wifi_rssi/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", WiFi.RSSI());
    mqttClient->publish(topic, value);

    // Uptime (in seconds)
    unsigned long uptimeSeconds = millis() / 1000;
    snprintf(topic, sizeof(topic), "%s/uptime/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%lu", uptimeSeconds);
    mqttClient->publish(topic, value);

    // Readable uptime (formatted)
    unsigned long days = uptimeSeconds / 86400;
    unsigned long hours = (uptimeSeconds % 86400) / 3600;
    unsigned long minutes = (uptimeSeconds % 3600) / 60;
    unsigned long seconds = uptimeSeconds % 60;

    snprintf(topic, sizeof(topic), "%s/uptime_readable/state", baseTopic.c_str());
    if (days > 0) {
        snprintf(value, sizeof(value), "%lud %luh %lum", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(value, sizeof(value), "%luh %lum", hours, minutes);
    } else {
        snprintf(value, sizeof(value), "%lum %lus", minutes, seconds);
    }
    mqttClient->publish(topic, value);

    // Image count
    snprintf(topic, sizeof(topic), "%s/image_count/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", configStorage.getImageSourceCount());
    mqttClient->publish(topic, value);

    // Current image index (1-based for display)
    snprintf(topic, sizeof(topic), "%s/current_image_index/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", configStorage.getCurrentImageIndex() + 1);
    mqttClient->publish(topic, value);

    // Cycling mode
    snprintf(topic, sizeof(topic), "%s/cycling_mode/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getCyclingEnabled() ? "Cycling" : "Single");

    // Random order status
    snprintf(topic, sizeof(topic), "%s/random_order_status/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getRandomOrder() ? "Enabled" : "Disabled");

    // Cycle interval (in seconds)
    snprintf(topic, sizeof(topic), "%s/cycle_interval_status/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%lu", configStorage.getCycleInterval() / 1000);
    mqttClient->publish(topic, value);

    // Update interval (in seconds)
    snprintf(topic, sizeof(topic), "%s/update_interval_status/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%lu", configStorage.getUpdateInterval() / 1000);
    mqttClient->publish(topic, value);

    // Display dimensions
    snprintf(topic, sizeof(topic), "%s/display_width/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", displayManager.getWidth());
    mqttClient->publish(topic, value);

    snprintf(topic, sizeof(topic), "%s/display_height/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", displayManager.getHeight());
    mqttClient->publish(topic, value);

    // Auto brightness status
    snprintf(topic, sizeof(topic), "%s/auto_brightness_status/state", baseTopic.c_str());
    mqttClient->publish(topic, configStorage.getBrightnessAutoMode() ? "Enabled" : "Disabled");

    // Brightness level
    snprintf(topic, sizeof(topic), "%s/brightness_level/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%d", displayManager.getBrightness());
    mqttClient->publish(topic, value);

    // Temperature (Celsius)
    float tempC = temperatureRead();
    snprintf(topic, sizeof(topic), "%s/temperature_celsius/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%.1f", tempC);
    mqttClient->publish(topic, value);

    // Temperature (Fahrenheit)
    snprintf(topic, sizeof(topic), "%s/temperature_fahrenheit/state", baseTopic.c_str());
    snprintf(value, sizeof(value), "%.1f", tempC * 9.0f / 5.0f + 32.0f);
    mqttClient->publish(topic, value);

    return true;
}

void HADiscovery::update() {
    if (!configStorage.getHADiscoveryEnabled()) {
        return;
    }

    // Drive the non-blocking discovery state machine
    if (_discoveryInProgress) {
        if (!mqttClient || !mqttClient->connected()) {
            LOG_WARNING("[HA] Discovery aborted - MQTT disconnected");
            _discoveryInProgress = false;
            _discoveryFailed = true;
            return;
        }

        unsigned long now = millis();
        if (now - _lastDiscoveryPublish < 50) {
            return;  // Rate limit: wait 50ms between publishes
        }

        LOG_DEBUG_F("[HA] Discovery step %d/%d\n", _discoveryStep + 1, HA_DISCOVERY_TOTAL_STEPS);
        bool result = publishDiscoveryStep(_discoveryStep);
        _lastDiscoveryPublish = millis();

        if (!result) {
            LOG_ERROR_F("[HA] Discovery step %d failed\n", _discoveryStep);
            _discoveryFailed = true;
            _discoveryInProgress = false;
            return;
        }

        _discoveryStep++;
        if (_discoveryStep >= HA_DISCOVERY_TOTAL_STEPS) {
            _discoveryInProgress = false;
            LOG_INFO("[HA] All discovery messages published successfully");

            // Subscribe to command topics now that discovery is complete
            String commandFilter = getCommandTopicFilter();
            LOG_DEBUG_F("[HA] Subscribing to HA commands: %s\n", commandFilter.c_str());
            if (!mqttClient->subscribe(commandFilter.c_str())) {
                LOG_ERROR_F("[HA] FAILED to subscribe to HA command topics! MQTT state: %d\n", mqttClient->state());
            } else {
                LOG_DEBUG("[HA] Subscribed to HA command topics");
            }

            // Publish initial state
            esp_task_wdt_reset();
            LOG_DEBUG("[HA] Publishing initial state to HA");
            publishState();
        }
        return;  // Only do one discovery step per update() call
    }

    // Normal periodic sensor updates
    unsigned long now = millis();
    unsigned long interval = configStorage.getHASensorUpdateInterval() * 1000; // Convert to milliseconds

    if (now - lastSensorUpdate >= interval) {
        lastSensorUpdate = now;
        publishSensors();
    }
}

void HADiscovery::handleCommand(const String& topic, const String& payload) {
    if (!configStorage.getHADiscoveryEnabled()) {
        return;
    }
    
    // Extract entity name from topic (format: baseTopic/entity/set)
    int lastSlash = topic.lastIndexOf('/');
    int secondLastSlash = topic.lastIndexOf('/', lastSlash - 1);
    String entity = topic.substring(secondLastSlash + 1, lastSlash);
    
    LOG_INFO_F("[HA] Command received - Entity: %s, Payload: %s\n", entity.c_str(), payload.c_str());

    // Validate payload length to prevent buffer overflows from malicious messages
    if (payload.length() > 256) {
        LOG_WARNING_F("[HA] Payload too long (%d bytes), ignoring command\n", payload.length());
        return;
    }

    // Handle brightness
    if (entity == "brightness") {
        // Ignore MQTT brightness commands when HA REST control is active
        if (configStorage.getUseHARestControl()) {
            LOG_INFO("[HA] Ignoring MQTT brightness command - HA REST Control is active\n");
            return;
        }
        
        int brightness = payload.toInt();
        if (brightness >= 0 && brightness <= 100) {
            LOG_INFO_F("[HA] Setting brightness to %d%%\n", brightness);
            displayManager.setBrightness(brightness);
            configStorage.setDefaultBrightness(brightness);
            configStorage.saveConfig();
            mqttClient->publish(buildStateTopic("brightness").c_str(), payload.c_str());
        } else {
            LOG_WARNING_F("[HA] Invalid brightness value: %d (must be 0-100)\n", brightness);
        }
    }
    // Handle switches
    else if (entity == "cycling") {
        bool enabled = (payload == "ON");
        LOG_INFO_F("[HA] Cycling mode: %s\n", enabled ? "enabled" : "disabled");
        configStorage.setCyclingEnabled(enabled);
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("cycling").c_str(), payload.c_str());
    }
    else if (entity == "random_order") {
        bool random = (payload == "ON");
        configStorage.setRandomOrder(random);
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("random_order").c_str(), payload.c_str());
    }
    else if (entity == "auto_brightness") {
        bool autoMode = (payload == "ON");
        configStorage.setBrightnessAutoMode(autoMode);
        
        if (autoMode) {
            LOG_INFO("[HA] MQTT brightness control enabled - auto-disabling HA REST Control");
            configStorage.setUseHARestControl(false);
        }
        
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("auto_brightness").c_str(), payload.c_str());
    }
    // Handle numbers (with range validation)
    else if (entity == "cycle_interval") {
        long rawValue = payload.toInt();
        if (rawValue < 1 || rawValue > 86400) {  // 1 second to 24 hours
            LOG_WARNING_F("[HA] Invalid cycle_interval: %ld (must be 1-86400 seconds)\n", rawValue);
            return;
        }
        unsigned long interval = (unsigned long)rawValue * 1000; // Convert seconds to milliseconds
        configStorage.setCycleInterval(interval);
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("cycle_interval").c_str(), payload.c_str());
    }
    else if (entity == "update_interval") {
        long rawValue = payload.toInt();
        if (rawValue < 5 || rawValue > 86400) {  // 5 seconds to 24 hours
            LOG_WARNING_F("[HA] Invalid update_interval: %ld (must be 5-86400 seconds)\n", rawValue);
            return;
        }
        unsigned long interval = (unsigned long)rawValue * 1000; // Convert seconds to milliseconds
        configStorage.setUpdateInterval(interval);
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("update_interval").c_str(), payload.c_str());
    }
    // Handle image source select
    else if (entity == "image_source") {
        // Extract image number from "Image X" format
        int imageNum = payload.substring(6).toInt(); // Skip "Image "
        int index = imageNum - 1; // Convert to 0-based index
        if (index >= 0 && index < configStorage.getImageSourceCount()) {
            configStorage.setCurrentImageIndex(index);
            configStorage.saveConfig();
            mqttClient->publish(buildStateTopic("image_source").c_str(), payload.c_str());
        }
    }
    // Handle buttons
    else if (entity == "reboot" && payload == "PRESS") {
        LOG_WARNING("[HA] Reboot requested via Home Assistant");
        delay(100);
        crashLogger.saveBeforeReboot();
        ESP.restart();
    }
    else if (entity == "next_image" && payload == "PRESS") {
        int nextIndex = (configStorage.getCurrentImageIndex() + 1) % configStorage.getImageSourceCount();
        LOG_INFO_F("[HA] Next image requested - switching to image %d\n", nextIndex + 1);
        configStorage.setCurrentImageIndex(nextIndex);
        configStorage.saveConfig();
        String imageSource = "Image " + String(nextIndex + 1);
        mqttClient->publish(buildStateTopic("image_source").c_str(), imageSource.c_str());
    }
    else if (entity == "reset_transforms" && payload == "PRESS") {
        configStorage.copyAllDefaultsToImageTransforms();
        configStorage.saveConfig();
        publishState(); // Update all transform states
    }
}
