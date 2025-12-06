#include "web_config.h"
#include "web_config_html.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// API handler functions for the web configuration interface
// These functions process POST requests and handle configuration changes

void WebConfig::handleSaveConfig() {
    bool needsRestart = false;
    bool brightnessChanged = false;
    bool imageSettingsChanged = false;
    int newBrightness = -1;
    
    // Parse form data and save configuration
    for (int i = 0; i < server->args(); i++) {
        String name = server->argName(i);
        String value = server->arg(i);
        
        // Network settings
        if (name == "wifi_ssid") {
            if (configStorage.getWiFiSSID() != value) needsRestart = true;
            configStorage.setWiFiSSID(value);
        }
        else if (name == "wifi_password") {
            if (configStorage.getWiFiPassword() != value) needsRestart = true;
            configStorage.setWiFiPassword(value);
        }
        
        // MQTT settings
        else if (name == "mqtt_server") {
            if (configStorage.getMQTTServer() != value) needsRestart = true;
            configStorage.setMQTTServer(value);
        }
        else if (name == "mqtt_port") {
            if (configStorage.getMQTTPort() != value.toInt()) needsRestart = true;
            configStorage.setMQTTPort(value.toInt());
        }
        else if (name == "mqtt_user") configStorage.setMQTTUser(value);
        else if (name == "mqtt_password") configStorage.setMQTTPassword(value);
        else if (name == "mqtt_client_id") configStorage.setMQTTClientID(value);
        
        // Home Assistant Discovery settings
        else if (name == "ha_device_name") configStorage.setHADeviceName(value);
        else if (name == "ha_discovery_prefix") configStorage.setHADiscoveryPrefix(value);
        else if (name == "ha_state_topic") configStorage.setHAStateTopic(value);
        else if (name == "ha_sensor_update_interval") configStorage.setHASensorUpdateInterval(value.toInt());
        
        // Image settings
        else if (name == "image_url") {
            if (configStorage.getImageURL() != value) imageSettingsChanged = true;
            configStorage.setImageURL(value);
        }
        
        // Display settings
        else if (name == "default_brightness") {
            int brightness = value.toInt();
            if (configStorage.getDefaultBrightness() != brightness) {
                brightnessChanged = true;
                newBrightness = brightness;
            }
            configStorage.setDefaultBrightness(brightness);
        }
        else if (name == "default_scale_x") {
            if (abs(configStorage.getDefaultScaleX() - value.toFloat()) > 0.01) imageSettingsChanged = true;
            configStorage.setDefaultScaleX(value.toFloat());
        }
        else if (name == "default_scale_y") {
            if (abs(configStorage.getDefaultScaleY() - value.toFloat()) > 0.01) imageSettingsChanged = true;
            configStorage.setDefaultScaleY(value.toFloat());
        }
        else if (name == "default_offset_x") {
            if (configStorage.getDefaultOffsetX() != value.toInt()) imageSettingsChanged = true;
            configStorage.setDefaultOffsetX(value.toInt());
        }
        else if (name == "default_offset_y") {
            if (configStorage.getDefaultOffsetY() != value.toInt()) imageSettingsChanged = true;
            configStorage.setDefaultOffsetY(value.toInt());
        }
        else if (name == "default_rotation") {
            if (abs(configStorage.getDefaultRotation() - value.toFloat()) > 0.01) imageSettingsChanged = true;
            configStorage.setDefaultRotation(value.toFloat());
        }
        else if (name == "backlight_freq") configStorage.setBacklightFreq(value.toInt());
        else if (name == "backlight_resolution") configStorage.setBacklightResolution(value.toInt());
        
        // Cycling settings
        else if (name == "cycle_interval") {
            unsigned long interval = value.toInt() * 1000UL;
            configStorage.setCycleInterval(interval);
        }
        
        // Advanced settings
        else if (name == "update_interval") {
            unsigned long newInterval = value.toInt() * 60UL * 1000UL;
            configStorage.setUpdateInterval(newInterval);
        }
        else if (name == "mqtt_reconnect_interval") configStorage.setMQTTReconnectInterval(value.toInt() * 1000);
        else if (name == "watchdog_timeout") configStorage.setWatchdogTimeout(value.toInt() * 1000);
        else if (name == "critical_heap_threshold") configStorage.setCriticalHeapThreshold(value.toInt());
        else if (name == "critical_psram_threshold") configStorage.setCriticalPSRAMThreshold(value.toInt());
    }
    
    // Handle checkbox parameters - HTML forms only send checked checkbox values
    // If a checkbox parameter is not present, it means the checkbox was unchecked
    configStorage.setCyclingEnabled(server->hasArg("cycling_enabled"));
    configStorage.setRandomOrder(server->hasArg("random_order"));
    configStorage.setBrightnessAutoMode(server->hasArg("brightness_auto_mode"));
    configStorage.setHADiscoveryEnabled(server->hasArg("ha_discovery_enabled"));
    
    // Save configuration to persistent storage
    configStorage.saveConfig();
    
    // Reload configuration in the running system
    reloadConfiguration();
    
    // Apply immediate changes
    if (brightnessChanged && newBrightness >= 0) {
        displayManager.setBrightness(newBrightness);
    }
    
    if (imageSettingsChanged) {
        applyImageSettings();
    }
    
    // Prepare response message
    String message = "Configuration saved successfully";
    if (needsRestart) message += " (restart required for network/MQTT changes)";
    if (brightnessChanged) message += " - brightness applied immediately";
    if (imageSettingsChanged) message += " - image settings applied immediately";
    
    String response = "{\"status\":\"success\",\"message\":\"" + message + "\"}";
    sendResponse(200, "application/json", response);
}

void WebConfig::handleRestart() {
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Device restarting now...\"}");
    
    displayManager.debugPrint("Device restart requested...", COLOR_YELLOW);
    
    delay(500);
    ESP.restart();
}

void WebConfig::handleAddImageSource() {
    if (server->hasArg("url")) {
        String url = server->arg("url");
        if (url.length() > 0) {
            configStorage.addImageSource(url);
            configStorage.saveConfig();
            sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source added successfully\"}");
        } else {
            sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid URL\"}");
        }
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"URL parameter required\"}");
    }
}

void WebConfig::handleRemoveImageSource() {
    if (server->hasArg("index")) {
        int index = server->arg("index").toInt();
        configStorage.removeImageSource(index);
        configStorage.saveConfig();
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source removed successfully\"}");
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
    }
}

void WebConfig::handleUpdateImageSource() {
    if (server->hasArg("index") && server->hasArg("url")) {
        int index = server->arg("index").toInt();
        String url = server->arg("url");
        configStorage.setImageSource(index, url);
        configStorage.saveConfig();
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source updated successfully\"}");
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index and URL parameters required\"}");
    }
}

void WebConfig::handleClearImageSources() {
    configStorage.clearImageSources();
    configStorage.addImageSource(configStorage.getImageURL());
    configStorage.saveConfig();
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"All image sources cleared, reset to single default source\"}");
}

void WebConfig::handleNextImage() {
    extern void advanceToNextImage();
    extern void updateCyclingVariables();
    extern void downloadAndDisplayImage();
    
    updateCyclingVariables();
    advanceToNextImage();
    downloadAndDisplayImage();
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Switched to next image and refreshed display\"}");
}

void WebConfig::handleUpdateImageTransform() {
    if (server->hasArg("index") && server->hasArg("property") && server->hasArg("value")) {
        int index = server->arg("index").toInt();
        String property = server->arg("property");
        String value = server->arg("value");
        
        bool success = true;
        String message = "Transform updated successfully";
        
        if (property == "scaleX") configStorage.setImageScaleX(index, value.toFloat());
        else if (property == "scaleY") configStorage.setImageScaleY(index, value.toFloat());
        else if (property == "offsetX") configStorage.setImageOffsetX(index, value.toInt());
        else if (property == "offsetY") configStorage.setImageOffsetY(index, value.toInt());
        else if (property == "rotation") configStorage.setImageRotation(index, value.toFloat());
        else {
            success = false;
            message = "Invalid property name";
        }
        
        if (success) {
            configStorage.saveConfig();
            
            if (index == configStorage.getCurrentImageIndex()) {
                extern float scaleX, scaleY;
                extern int16_t offsetX, offsetY;
                extern float rotationAngle;
                extern void renderFullImage();
                
                if (property == "scaleX") scaleX = configStorage.getImageScaleX(index);
                else if (property == "scaleY") scaleY = configStorage.getImageScaleY(index);
                else if (property == "offsetX") offsetX = configStorage.getImageOffsetX(index);
                else if (property == "offsetY") offsetY = configStorage.getImageOffsetY(index);
                else if (property == "rotation") rotationAngle = configStorage.getImageRotation(index);
                
                renderFullImage();
            }
        }
        
        String response = "{\"status\":\"" + String(success ? "success" : "error") + "\",\"message\":\"" + message + "\"}";
        sendResponse(200, "application/json", response);
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
    }
}

void WebConfig::handleCopyDefaultsToImage() {
    if (server->hasArg("index")) {
        int index = server->arg("index").toInt();
        
        configStorage.copyDefaultsToImageTransform(index);
        configStorage.saveConfig();
        
        if (index == configStorage.getCurrentImageIndex()) {
            extern float scaleX, scaleY;
            extern int16_t offsetX, offsetY;
            extern float rotationAngle;
            extern void renderFullImage();
            
            scaleX = configStorage.getImageScaleX(index);
            scaleY = configStorage.getImageScaleY(index);
            offsetX = configStorage.getImageOffsetX(index);
            offsetY = configStorage.getImageOffsetY(index);
            rotationAngle = configStorage.getImageRotation(index);
            
            renderFullImage();
            Serial.println("Applied global defaults to current image");
        }
        
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Default settings copied to image\"}");
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
    }
}

void WebConfig::handleApplyTransform() {
    if (server->hasArg("index")) {
        int index = server->arg("index").toInt();
        int currentIndex = configStorage.getCurrentImageIndex();
        
        if (index != currentIndex) {
            configStorage.setCurrentImageIndex(index);
            configStorage.saveConfig();
            
            extern void downloadAndDisplayImage();
            downloadAndDisplayImage();
        } else {
            extern float scaleX, scaleY;
            extern int16_t offsetX, offsetY;
            extern float rotationAngle;
            extern void renderFullImage();
            
            scaleX = configStorage.getImageScaleX(index);
            scaleY = configStorage.getImageScaleY(index);
            offsetX = configStorage.getImageOffsetX(index);
            offsetY = configStorage.getImageOffsetY(index);
            rotationAngle = configStorage.getImageRotation(index);
            
            renderFullImage();
        }
        
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Transform applied successfully\"}");
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
    }
}

void WebConfig::handleFactoryReset() {
    configStorage.resetToDefaults();
    
    // Clear WiFi credentials and provisioning flag to trigger captive portal on next boot
    configStorage.setWiFiSSID("");
    configStorage.setWiFiPassword("");
    configStorage.setWiFiProvisioned(false);
    configStorage.saveConfig();
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset completed. WiFi setup will run on next boot. Device restarting...\"}");
    
    displayManager.debugPrint("Factory reset in progress...", COLOR_YELLOW);
    displayManager.debugPrint("WiFi setup portal will run on restart", COLOR_CYAN);
    
    delay(500);
    ESP.restart();
}

void WebConfig::applyImageSettings() {
    extern float scaleX, scaleY;
    extern int16_t offsetX, offsetY;
    extern float rotationAngle;
    extern void renderFullImage();
    
    scaleX = configStorage.getDefaultScaleX();
    scaleY = configStorage.getDefaultScaleY();
    offsetX = configStorage.getDefaultOffsetX();
    offsetY = configStorage.getDefaultOffsetY();
    rotationAngle = configStorage.getDefaultRotation();
    
    renderFullImage();
}

void WebConfig::reloadConfiguration() {
    extern void updateCyclingVariables();
    
    Serial.println("Reloading configuration from web interface...");
    updateCyclingVariables();
    
    extern unsigned long currentUpdateInterval;
    extern unsigned long currentCycleInterval;
    extern bool cyclingEnabled;
    extern bool randomOrderEnabled;
    extern int imageSourceCount;
    
    currentUpdateInterval = configStorage.getUpdateInterval();
    currentCycleInterval = configStorage.getCycleInterval();
    cyclingEnabled = configStorage.getCyclingEnabled();
    randomOrderEnabled = configStorage.getRandomOrder();
    imageSourceCount = configStorage.getImageSourceCount();
}

void WebConfig::handleGetAllInfo() {
    // Comprehensive device information API endpoint
    String json = "{";
    
    // Firmware information
    json += "\"firmware\":{";
    json += "\"sketch_size\":" + String(ESP.getSketchSize()) + ",";
    json += "\"free_sketch_space\":" + String(ESP.getFreeSketchSpace()) + ",";
    json += "\"sketch_md5\":\"" + String(ESP.getSketchMD5()) + "\"";
    json += "},";
    
    // System information
    json += "\"system\":{";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"uptime_seconds\":" + String(millis() / 1000) + ",";
    json += "\"free_heap\":" + String(systemMonitor.getCurrentFreeHeap()) + ",";
    json += "\"total_heap\":" + String(ESP.getHeapSize()) + ",";
    json += "\"min_free_heap\":" + String(systemMonitor.getMinFreeHeap()) + ",";
    json += "\"free_psram\":" + String(systemMonitor.getCurrentFreePsram()) + ",";
    json += "\"total_psram\":" + String(ESP.getPsramSize()) + ",";
    json += "\"min_free_psram\":" + String(systemMonitor.getMinFreePsram()) + ",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
    json += "\"flash_speed\":" + String(ESP.getFlashChipSpeed()) + ",";
    json += "\"chip_model\":\"" + String(ESP.getChipModel()) + "\",";
    json += "\"chip_revision\":" + String(ESP.getChipRevision()) + ",";
    json += "\"chip_cores\":" + String(ESP.getChipCores()) + ",";
    json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"sdk_version\":\"" + String(ESP.getSdkVersion()) + "\",";
    json += "\"temperature_celsius\":" + String(temperatureRead(), 1) + ",";
    json += "\"temperature_fahrenheit\":" + String(temperatureRead() * 9.0 / 5.0 + 32.0, 1) + ",";
    json += "\"healthy\":" + String(systemMonitor.isSystemHealthy() ? "true" : "false");
    json += "},";
    
    // Network information
    json += "\"network\":{";
    json += "\"connected\":" + String(wifiManager.isConnected() ? "true" : "false") + ",";
    if (wifiManager.isConnected()) {
        json += "\"ssid\":\"" + escapeJson(String(WiFi.SSID())) + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
        json += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
        json += "\"mac\":\"" + WiFi.macAddress() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"hostname\":\"" + String(WiFi.getHostname()) + "\"";
    } else {
        json += "\"ssid\":null,";
        json += "\"ip\":null,";
        json += "\"gateway\":null,";
        json += "\"dns\":null,";
        json += "\"mac\":\"" + WiFi.macAddress() + "\",";
        json += "\"rssi\":0,";
        json += "\"hostname\":null";
    }
    json += "},";
    
    // MQTT information
    json += "\"mqtt\":{";
    json += "\"connected\":" + String(mqttManager.isConnected() ? "true" : "false") + ",";
    json += "\"server\":\"" + escapeJson(configStorage.getMQTTServer()) + "\",";
    json += "\"port\":" + String(configStorage.getMQTTPort()) + ",";
    json += "\"client_id\":\"" + escapeJson(configStorage.getMQTTClientID()) + "\",";
    json += "\"username\":\"" + escapeJson(configStorage.getMQTTUser()) + "\"";
    json += "},";
    
    // Home Assistant Discovery information
    json += "\"home_assistant\":{";
    json += "\"discovery_enabled\":" + String(configStorage.getHADiscoveryEnabled() ? "true" : "false") + ",";
    json += "\"device_name\":\"" + escapeJson(configStorage.getHADeviceName()) + "\",";
    json += "\"discovery_prefix\":\"" + escapeJson(configStorage.getHADiscoveryPrefix()) + "\",";
    json += "\"state_topic\":\"" + escapeJson(configStorage.getHAStateTopic()) + "\",";
    json += "\"sensor_update_interval\":" + String(configStorage.getHASensorUpdateInterval());
    json += "},";
    
    // Display information
    json += "\"display\":{";
    json += "\"width\":" + String(displayManager.getWidth()) + ",";
    json += "\"height\":" + String(displayManager.getHeight()) + ",";
    json += "\"brightness\":" + String(displayManager.getBrightness()) + ",";
    json += "\"brightness_auto_mode\":" + String(configStorage.getBrightnessAutoMode() ? "true" : "false") + ",";
    json += "\"backlight_freq\":" + String(configStorage.getBacklightFreq()) + ",";
    json += "\"backlight_resolution\":" + String(configStorage.getBacklightResolution());
    json += "},";
    
    // Image configuration
    json += "\"image\":{";
    json += "\"cycling_enabled\":" + String(configStorage.getCyclingEnabled() ? "true" : "false") + ",";
    json += "\"update_interval\":" + String(configStorage.getUpdateInterval()) + ",";
    json += "\"current_url\":\"" + escapeJson(configStorage.getCurrentImageURL()) + "\",";
    
    if (configStorage.getCyclingEnabled()) {
        json += "\"cycle_interval\":" + String(configStorage.getCycleInterval()) + ",";
        json += "\"random_order\":" + String(configStorage.getRandomOrder() ? "true" : "false") + ",";
        json += "\"current_index\":" + String(configStorage.getCurrentImageIndex()) + ",";
        json += "\"source_count\":" + String(configStorage.getImageSourceCount()) + ",";
        json += "\"sources\":[";
        int count = configStorage.getImageSourceCount();
        for (int i = 0; i < count; i++) {
            if (i > 0) json += ",";
            json += "{";
            json += "\"index\":" + String(i) + ",";
            json += "\"url\":\"" + escapeJson(configStorage.getImageSource(i)) + "\",";
            json += "\"active\":" + String(i == configStorage.getCurrentImageIndex() ? "true" : "false") + ",";
            json += "\"scale_x\":" + String(configStorage.getImageScaleX(i), 4) + ",";
            json += "\"scale_y\":" + String(configStorage.getImageScaleY(i), 4) + ",";
            json += "\"offset_x\":" + String(configStorage.getImageOffsetX(i)) + ",";
            json += "\"offset_y\":" + String(configStorage.getImageOffsetY(i)) + ",";
            json += "\"rotation\":" + String(configStorage.getImageRotation(i), 4);
            json += "}";
        }
        json += "]";
    } else {
        json += "\"url\":\"" + escapeJson(configStorage.getImageURL()) + "\"";
    }
    json += "},";
    
    // Default transformation settings
    json += "\"defaults\":{";
    json += "\"brightness\":" + String(configStorage.getDefaultBrightness()) + ",";
    json += "\"scale_x\":" + String(configStorage.getDefaultScaleX(), 4) + ",";
    json += "\"scale_y\":" + String(configStorage.getDefaultScaleY(), 4) + ",";
    json += "\"offset_x\":" + String(configStorage.getDefaultOffsetX()) + ",";
    json += "\"offset_y\":" + String(configStorage.getDefaultOffsetY()) + ",";
    json += "\"rotation\":" + String(configStorage.getDefaultRotation(), 4);
    json += "},";
    
    // Advanced settings
    json += "\"advanced\":{";
    json += "\"mqtt_reconnect_interval\":" + String(configStorage.getMQTTReconnectInterval()) + ",";
    json += "\"watchdog_timeout\":" + String(configStorage.getWatchdogTimeout()) + ",";
    json += "\"critical_heap_threshold\":" + String(configStorage.getCriticalHeapThreshold()) + ",";
    json += "\"critical_psram_threshold\":" + String(configStorage.getCriticalPSRAMThreshold());
    json += "}";
    
    json += "}";
    
    sendResponse(200, "application/json", json);
}
