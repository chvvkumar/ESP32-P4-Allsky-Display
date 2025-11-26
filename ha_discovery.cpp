#include "ha_discovery.h"
#include "display_manager.h"
#include "system_monitor.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

// Global instance
HADiscovery haDiscovery;

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
    json += "\"manufacturer\":\"Custom\",";
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
    
    Serial.printf("Publishing light discovery (size: %d bytes)\n", payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish light discovery! Payload size: %d\n", payload.length());
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
    
    Serial.printf("Publishing switch '%s' (size: %d bytes)\n", entityId, payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish switch '%s'! Payload size: %d\n", entityId, payload.length());
    }
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
    
    Serial.printf("Publishing number '%s' (size: %d bytes)\n", entityId, payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish number '%s'! Payload size: %d\n", entityId, payload.length());
    }
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
    
    Serial.printf("Publishing select 'image_source' (size: %d bytes)\n", payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish select! Payload size: %d\n", payload.length());
    }
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
    
    Serial.printf("Publishing button '%s' (size: %d bytes)\n", entityId, payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish button '%s'! Payload size: %d\n", entityId, payload.length());
    }
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
    
    Serial.printf("Publishing sensor '%s' (size: %d bytes)\n", entityId, payload.length());
    bool result = mqttClient->publish(topic.c_str(), payload.c_str(), true);
    if (!result) {
        Serial.printf("ERROR: Failed to publish sensor '%s'! Payload size: %d\n", entityId, payload.length());
    }
    return result;
}

bool HADiscovery::publishImageTransformEntities() {
    int imageCount = configStorage.getImageSourceCount();
    
    for (int i = 0; i < imageCount; i++) {
        String imagePrefix = "img" + String(i) + "_";
        String imageName = "Image " + String(i + 1) + " ";
        
        // Scale X
        if (!publishNumberDiscovery((imagePrefix + "scale_x").c_str(), 
                                    (imageName + "Scale X").c_str(),
                                    0.1, 3.0, 0.1, "", "mdi:arrow-expand-horizontal")) {
            return false;
        }
        delay(10);
        
        // Scale Y
        if (!publishNumberDiscovery((imagePrefix + "scale_y").c_str(),
                                    (imageName + "Scale Y").c_str(),
                                    0.1, 3.0, 0.1, "", "mdi:arrow-expand-vertical")) {
            return false;
        }
        delay(10);
        
        // Offset X
        if (!publishNumberDiscovery((imagePrefix + "offset_x").c_str(),
                                    (imageName + "Offset X").c_str(),
                                    -500, 500, 10, "px", "mdi:arrow-left-right")) {
            return false;
        }
        delay(10);
        
        // Offset Y
        if (!publishNumberDiscovery((imagePrefix + "offset_y").c_str(),
                                    (imageName + "Offset Y").c_str(),
                                    -500, 500, 10, "px", "mdi:arrow-up-down")) {
            return false;
        }
        delay(10);
        
        // Rotation
        if (!publishNumberDiscovery((imagePrefix + "rotation").c_str(),
                                    (imageName + "Rotation").c_str(),
                                    0, 360, 90, "°", "mdi:rotate-right")) {
            return false;
        }
        delay(10);
    }
    
    return true;
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
    if (!publishNumberDiscovery("update_interval", "Update Interval", 60, 3600, 60, "s", "mdi:update")) return false;
    delay(50);
    
    // Per-image transform entities
    if (!publishImageTransformEntities()) {
        Serial.println("Failed to publish image transform entities");
        return false;
    }
    
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
    
    Serial.println("Home Assistant discovery complete");
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
    
    // Per-image transforms
    int imageCount = configStorage.getImageSourceCount();
    for (int i = 0; i < imageCount; i++) {
        String imagePrefix = "img" + String(i) + "_";
        
        String scaleX = String(configStorage.getImageScaleX(i), 2);
        mqttClient->publish(buildStateTopic((imagePrefix + "scale_x").c_str()).c_str(), scaleX.c_str());
        
        String scaleY = String(configStorage.getImageScaleY(i), 2);
        mqttClient->publish(buildStateTopic((imagePrefix + "scale_y").c_str()).c_str(), scaleY.c_str());
        
        String offsetX = String(configStorage.getImageOffsetX(i));
        mqttClient->publish(buildStateTopic((imagePrefix + "offset_x").c_str()).c_str(), offsetX.c_str());
        
        String offsetY = String(configStorage.getImageOffsetY(i));
        mqttClient->publish(buildStateTopic((imagePrefix + "offset_y").c_str()).c_str(), offsetY.c_str());
        
        String rotation = String(configStorage.getImageRotation(i), 0);
        mqttClient->publish(buildStateTopic((imagePrefix + "rotation").c_str()).c_str(), rotation.c_str());
    }
    
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
    
    Serial.println("MQTT: Sensor data published to Home Assistant");
    
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
    
    Serial.printf("HA Command - Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());
    
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
        Serial.println("Reboot requested via Home Assistant");
        delay(100);
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
    // Handle per-image transforms
    else if (entity.startsWith("img")) {
        // Parse image index from entity name (e.g., "img0_scale_x")
        int underscorePos = entity.indexOf('_');
        if (underscorePos > 3) {
            int imageIndex = entity.substring(3, underscorePos).toInt();
            String transformType = entity.substring(underscorePos + 1);
            
            if (transformType == "scale_x") {
                float value = payload.toFloat();
                configStorage.setImageScaleX(imageIndex, value);
                configStorage.saveConfig();
                mqttClient->publish(buildStateTopic(entity.c_str()).c_str(), payload.c_str());
            }
            else if (transformType == "scale_y") {
                float value = payload.toFloat();
                configStorage.setImageScaleY(imageIndex, value);
                configStorage.saveConfig();
                mqttClient->publish(buildStateTopic(entity.c_str()).c_str(), payload.c_str());
            }
            else if (transformType == "offset_x") {
                int value = payload.toInt();
                configStorage.setImageOffsetX(imageIndex, value);
                configStorage.saveConfig();
                mqttClient->publish(buildStateTopic(entity.c_str()).c_str(), payload.c_str());
            }
            else if (transformType == "offset_y") {
                int value = payload.toInt();
                configStorage.setImageOffsetY(imageIndex, value);
                configStorage.saveConfig();
                mqttClient->publish(buildStateTopic(entity.c_str()).c_str(), payload.c_str());
            }
            else if (transformType == "rotation") {
                float value = payload.toFloat();
                configStorage.setImageRotation(imageIndex, value);
                configStorage.saveConfig();
                mqttClient->publish(buildStateTopic(entity.c_str()).c_str(), payload.c_str());
            }
        }
    }
}
