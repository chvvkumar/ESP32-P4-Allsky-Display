#include "ha_discovery.h"
#include "display_manager.h"
#include "system_monitor.h"
#include "crash_logger.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Global instances
HADiscovery haDiscovery;
extern CrashLogger crashLogger;

HADiscovery::HADiscovery() :
    mqttClient(nullptr),
    lastSensorUpdate(0),
    lastSensorPublish(0)
{
}

void HADiscovery::begin(PubSubClient* client) {
    mqttClient = client;
    deviceId = getDeviceId();
    baseTopic = getBaseTopic();
}

String HADiscovery::getDeviceId() {
    // Use MAC address as unique device ID
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char macStr[13];
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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
    return baseTopic + "/attributes";
}

String HADiscovery::getAvailabilityTopic() {
    return baseTopic + "/availability";
}

String HADiscovery::getCommandTopicFilter() {
    return baseTopic + "/+/set";
}

String HADiscovery::getDeviceJson() {
    String json = "{";
    json += "\"identifiers\":[\"" + deviceId + "\"],";
    json += "\"name\":\"" + configStorage.getHADeviceName() + "\",";
    json += "\"model\":\"ESP32-P4-WIFI6-Touch-LCD\",";
    json += "\"manufacturer\":\"chvvkumar\",";
    json += "\"sw_version\":\"1.0\"";
    json += "}";
    return json;
}

bool HADiscovery::publishLightDiscovery() {
    String topic = buildDiscoveryTopic("light", "brightness");
    String payload = "{";
    payload += "\"name\":\"Brightness\",";
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
        Serial.println("ERROR: Failed to publish light discovery!");
    }
    return result;
}

bool HADiscovery::publishSwitchDiscovery(const char* entityId, const char* name, const char* icon) {
    String topic = buildDiscoveryTopic("switch", entityId);
    String payload = "{";
    payload += "\"name\":\"" + String(name) + "\",";
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
    payload += "\"name\":\"" + String(name) + "\",";
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
    payload += "\"name\":\"Image Source\",";
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
    payload += "\"name\":\"" + String(name) + "\",";
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
    payload += "\"name\":\"" + String(name) + "\",";
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

bool HADiscovery::publishDiscovery() {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    if (!configStorage.getHADiscoveryEnabled()) {
        return false;
    }
    
    Serial.println("Publishing Home Assistant discovery messages...");
    
    // Light entity (brightness)
    if (!publishLightDiscovery()) {
        Serial.println("Failed to publish light discovery");
        return false;
    }
    delay(50);
    
    // Switch entities
    if (!publishSwitchDiscovery("cycling", "Cycling Enabled", "mdi:image-multiple")) return false;
    delay(50);
    if (!publishSwitchDiscovery("random_order", "Random Order", "mdi:shuffle")) return false;
    delay(50);
    if (!publishSwitchDiscovery("auto_brightness", "Auto Brightness", "mdi:brightness-auto")) return false;
    delay(50);
    
    // Number entities
    if (!publishNumberDiscovery("cycle_interval", "Cycle Interval", 10, 3600, 10, "s", "mdi:timer")) return false;
    delay(50);
    if (!publishNumberDiscovery("update_interval", "Update Interval", 10, 3600, 10, "s", "mdi:update")) return false;
    delay(50);
    
    // Select entity (image source)
    if (!publishSelectDiscovery()) return false;
    delay(50);
    
    // Button entities
    if (!publishButtonDiscovery("reboot", "Reboot", "mdi:restart")) return false;
    delay(50);
    if (!publishButtonDiscovery("next_image", "Next Image", "mdi:skip-next")) return false;
    delay(50);
    if (!publishButtonDiscovery("reset_transforms", "Reset Transforms", "mdi:restore")) return false;
    delay(50);
    
    // Sensor entities
    if (!publishSensorDiscovery("current_image", "Current Image URL", "", "", "mdi:image")) return false;
    delay(50);
    if (!publishSensorDiscovery("free_heap", "Free Heap", "KB", "", "mdi:memory")) return false;
    delay(50);
    if (!publishSensorDiscovery("free_psram", "Free PSRAM", "KB", "", "mdi:memory")) return false;
    delay(50);
    if (!publishSensorDiscovery("wifi_rssi", "WiFi Signal", "dBm", "signal_strength", "mdi:wifi")) return false;
    delay(50);
    if (!publishSensorDiscovery("uptime", "Uptime", "s", "", "mdi:clock-outline")) return false;
    delay(50);
    if (!publishSensorDiscovery("image_count", "Image Count", "", "", "mdi:counter")) return false;
    delay(50);
    if (!publishSensorDiscovery("current_image_index", "Current Image Index", "", "", "mdi:numeric")) return false;
    delay(50);
    if (!publishSensorDiscovery("cycling_mode", "Cycling Mode", "", "", "mdi:sync")) return false;
    delay(50);
    if (!publishSensorDiscovery("random_order_status", "Random Order", "", "", "mdi:shuffle-variant")) return false;
    delay(50);
    if (!publishSensorDiscovery("cycle_interval_status", "Cycle Interval", "s", "", "mdi:timer-outline")) return false;
    delay(50);
    if (!publishSensorDiscovery("update_interval_status", "Update Interval", "s", "", "mdi:update")) return false;
    delay(50);
    if (!publishSensorDiscovery("display_width", "Display Width", "px", "", "mdi:monitor-screenshot")) return false;
    delay(50);
    if (!publishSensorDiscovery("display_height", "Display Height", "px", "", "mdi:monitor-screenshot")) return false;
    delay(50);
    if (!publishSensorDiscovery("auto_brightness_status", "Auto Brightness", "", "", "mdi:brightness-auto")) return false;
    delay(50);
    if (!publishSensorDiscovery("brightness_level", "Brightness Level", "%", "", "mdi:brightness-6")) return false;
    delay(50);
    if (!publishSensorDiscovery("temperature_celsius", "Temperature", "°C", "temperature", "mdi:thermometer")) return false;
    delay(50);
    if (!publishSensorDiscovery("temperature_fahrenheit", "Temperature (F)", "°F", "temperature", "mdi:thermometer")) return false;
    delay(50);
    
    return true;
}

bool HADiscovery::publishAvailability(bool online) {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    String topic = getAvailabilityTopic();
    const char* payload = online ? "online" : "offline";
    return mqttClient->publish(topic.c_str(), payload, true);
}

bool HADiscovery::publishState() {
    if (!mqttClient || !mqttClient->connected()) {
        return false;
    }
    
    if (!configStorage.getHADiscoveryEnabled()) {
        return false;
    }
    
    // Publish individual entity states
    
    // Brightness
    String brightness = String(displayManager.getBrightness());
    mqttClient->publish(buildStateTopic("brightness").c_str(), brightness.c_str());
    
    // Cycling
    mqttClient->publish(buildStateTopic("cycling").c_str(), 
                       configStorage.getCyclingEnabled() ? "ON" : "OFF");
    
    // Random order
    mqttClient->publish(buildStateTopic("random_order").c_str(),
                       configStorage.getRandomOrder() ? "ON" : "OFF");
    
    // Auto brightness
    mqttClient->publish(buildStateTopic("auto_brightness").c_str(),
                       configStorage.getBrightnessAutoMode() ? "ON" : "OFF");
    
    // Cycle interval (convert milliseconds to seconds)
    String cycleInterval = String(configStorage.getCycleInterval() / 1000);
    mqttClient->publish(buildStateTopic("cycle_interval").c_str(), cycleInterval.c_str());
    
    // Update interval (convert milliseconds to seconds)
    String updateInterval = String(configStorage.getUpdateInterval() / 1000);
    mqttClient->publish(buildStateTopic("update_interval").c_str(), updateInterval.c_str());
    
    // Image source
    int currentIndex = configStorage.getCurrentImageIndex();
    String imageSource = "Image " + String(currentIndex + 1);
    mqttClient->publish(buildStateTopic("image_source").c_str(), imageSource.c_str());
    
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
    
    // Current image URL
    String currentImageURL = configStorage.getCurrentImageURL();
    mqttClient->publish(buildStateTopic("current_image").c_str(), currentImageURL.c_str());
    
    // Free heap (in KB)
    String freeHeap = String(ESP.getFreeHeap() / 1024);
    mqttClient->publish(buildStateTopic("free_heap").c_str(), freeHeap.c_str());
    
    // Free PSRAM (in KB)
    String freePSRAM = String(ESP.getFreePsram() / 1024);
    mqttClient->publish(buildStateTopic("free_psram").c_str(), freePSRAM.c_str());
    
    // WiFi RSSI
    String rssi = String(WiFi.RSSI());
    mqttClient->publish(buildStateTopic("wifi_rssi").c_str(), rssi.c_str());
    
    // Uptime (in seconds)
    String uptime = String(millis() / 1000);
    mqttClient->publish(buildStateTopic("uptime").c_str(), uptime.c_str());
    
    // Image count
    String imageCount = String(configStorage.getImageSourceCount());
    mqttClient->publish(buildStateTopic("image_count").c_str(), imageCount.c_str());
    
    // Current image index (1-based for display)
    String currentIndex = String(configStorage.getCurrentImageIndex() + 1);
    mqttClient->publish(buildStateTopic("current_image_index").c_str(), currentIndex.c_str());
    
    // Cycling mode
    String cyclingMode = configStorage.getCyclingEnabled() ? "Cycling" : "Single";
    mqttClient->publish(buildStateTopic("cycling_mode").c_str(), cyclingMode.c_str());
    
    // Random order status
    String randomOrder = configStorage.getRandomOrder() ? "Enabled" : "Disabled";
    mqttClient->publish(buildStateTopic("random_order_status").c_str(), randomOrder.c_str());
    
    // Cycle interval (in seconds)
    String cycleIntervalStatus = String(configStorage.getCycleInterval() / 1000);
    mqttClient->publish(buildStateTopic("cycle_interval_status").c_str(), cycleIntervalStatus.c_str());
    
    // Update interval (in seconds)
    String updateIntervalStatus = String(configStorage.getUpdateInterval() / 1000);
    mqttClient->publish(buildStateTopic("update_interval_status").c_str(), updateIntervalStatus.c_str());
    
    // Display dimensions
    String displayWidth = String(displayManager.getWidth());
    mqttClient->publish(buildStateTopic("display_width").c_str(), displayWidth.c_str());
    
    String displayHeight = String(displayManager.getHeight());
    mqttClient->publish(buildStateTopic("display_height").c_str(), displayHeight.c_str());
    
    // Auto brightness status
    String autoBrightness = configStorage.getBrightnessAutoMode() ? "Enabled" : "Disabled";
    mqttClient->publish(buildStateTopic("auto_brightness_status").c_str(), autoBrightness.c_str());
    
    // Brightness level
    String brightnessLevel = String(displayManager.getBrightness());
    mqttClient->publish(buildStateTopic("brightness_level").c_str(), brightnessLevel.c_str());
    
    // Temperature (Celsius)
    String tempCelsius = String(temperatureRead(), 1);
    mqttClient->publish(buildStateTopic("temperature_celsius").c_str(), tempCelsius.c_str());
    
    // Temperature (Fahrenheit)
    String tempFahrenheit = String(temperatureRead() * 9.0 / 5.0 + 32.0, 1);
    mqttClient->publish(buildStateTopic("temperature_fahrenheit").c_str(), tempFahrenheit.c_str());
    
    return true;
}

void HADiscovery::update() {
    if (!configStorage.getHADiscoveryEnabled()) {
        return;
    }
    
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
    
    // Handle brightness
    if (entity == "brightness") {
        int brightness = payload.toInt();
        if (brightness >= 0 && brightness <= 100) {
            displayManager.setBrightness(brightness);
            configStorage.setDefaultBrightness(brightness);
            configStorage.saveConfig();
            mqttClient->publish(buildStateTopic("brightness").c_str(), payload.c_str());
        }
    }
    // Handle switches
    else if (entity == "cycling") {
        bool enabled = (payload == "ON");
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
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("auto_brightness").c_str(), payload.c_str());
    }
    // Handle numbers
    else if (entity == "cycle_interval") {
        unsigned long interval = payload.toInt() * 1000; // Convert seconds to milliseconds
        configStorage.setCycleInterval(interval);
        configStorage.saveConfig();
        mqttClient->publish(buildStateTopic("cycle_interval").c_str(), payload.c_str());
    }
    else if (entity == "update_interval") {
        unsigned long interval = payload.toInt() * 1000; // Convert seconds to milliseconds
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
        delay(100);
        crashLogger.saveBeforeReboot();
        ESP.restart();
    }
    else if (entity == "next_image" && payload == "PRESS") {
        int nextIndex = (configStorage.getCurrentImageIndex() + 1) % configStorage.getImageSourceCount();
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
