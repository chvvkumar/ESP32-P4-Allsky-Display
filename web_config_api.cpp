#include "web_config.h"
#include "web_config_html.h"
#include "system_monitor.h"
#include "config_storage.h"
#include "network_manager.h"
#include "crash_logger.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "ota_manager.h"
#include "device_health.h"
#include "ha_rest_client.h"
#include "ha_discovery.h"
#include "logging.h"
#include <Update.h>

// External global instances
extern CrashLogger crashLogger;

// System monitor is needed for watchdog resets during OTA

// API handler functions for the web configuration interface
// These functions process POST requests and handle configuration changes

void WebConfig::handleSaveConfig() {
    LOG_INFO("[WebAPI] Configuration save request received");
    
    bool needsRestart = false;
    bool brightnessChanged = false;
    bool imageSettingsChanged = false;
    bool displayTypeChanged = false;
    int newBrightness = -1;
    
    // Parse form data and save configuration
    for (int i = 0; i < server->args(); i++) {
        String name = server->argName(i);
        String value = server->arg(i);
        
        // Network settings
        if (name == "wifi_ssid") {
            if (configStorage.getWiFiSSID() != value) {
                LOG_INFO_F("[WebAPI] WiFi SSID updated: %s (restart required)\n", value.c_str());
                needsRestart = true;
            }
            configStorage.setWiFiSSID(value);
        }
        else if (name == "wifi_password") {
            if (configStorage.getWiFiPassword() != value) {
                LOG_INFO("[WebAPI] WiFi password updated (value hidden for security) - restart required");
                needsRestart = true;
            }
            configStorage.setWiFiPassword(value);
        }
        
        // MQTT settings
        else if (name == "mqtt_server") {
            if (configStorage.getMQTTServer() != value) {
                LOG_INFO_F("[WebAPI] MQTT server updated: %s (restart required)\n", value.c_str());
                needsRestart = true;
            }
            configStorage.setMQTTServer(value);
        }
        else if (name == "mqtt_port") {
            if (configStorage.getMQTTPort() != value.toInt()) {
                LOG_INFO_F("[WebAPI] MQTT port updated: %d (restart required)\n", value.toInt());
                needsRestart = true;
            }
            configStorage.setMQTTPort(value.toInt());
        }
        else if (name == "mqtt_user") {
            LOG_DEBUG_F("[WebAPI] MQTT username updated: %s\n", value.c_str());
            configStorage.setMQTTUser(value);
        }
        else if (name == "mqtt_password") {
            LOG_DEBUG("[WebAPI] MQTT password updated (value hidden for security)");
            configStorage.setMQTTPassword(value);
        }
        else if (name == "mqtt_client_id") configStorage.setMQTTClientID(value);
        
        // Home Assistant Discovery settings
        else if (name == "ha_device_name") configStorage.setHADeviceName(value);
        else if (name == "ha_discovery_prefix") configStorage.setHADiscoveryPrefix(value);
        else if (name == "ha_state_topic") configStorage.setHAStateTopic(value);
        else if (name == "ha_sensor_update_interval") configStorage.setHASensorUpdateInterval(value.toInt());
        
        // Image settings
        else if (name == "image_url") {
            if (configStorage.getImageURL() != value) {
                LOG_INFO_F("[WebAPI] Image URL updated: %s\n", value.c_str());
                imageSettingsChanged = true;
            }
            configStorage.setImageURL(value);
        }
        
        // Display settings
        else if (name == "default_brightness") {
            int brightness = value.toInt();
            if (configStorage.getDefaultBrightness() != brightness) {
                LOG_INFO_F("[WebAPI] Brightness updated: %d%% (applied immediately)\n", brightness);
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
        else if (name == "color_temp") {
            int temp = value.toInt();
            configStorage.setColorTemp(temp);
            LOG_INFO_F("[WebAPI] Color temperature set to %dK\n", temp);
            imageSettingsChanged = true;  // Trigger image refresh to apply new color temp
        }
        
        // Cycling settings
        else if (name == "cycle_interval") {
            unsigned long interval = value.toInt() * 1000UL;
            configStorage.setCycleInterval(interval);
        }
        else if (name == "image_update_mode") {
            int mode = value.toInt();
            configStorage.setImageUpdateMode(mode);
            LOG_INFO_F("[WebAPI] Image update mode changed to: %s\n", mode == 0 ? "Automatic Cycling" : "API-Triggered Refresh");
        }
        else if (name == "default_image_duration") {
            unsigned long duration = value.toInt();
            configStorage.setDefaultImageDuration(duration);
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
        
        // Display hardware settings
        else if (name == "display_type") {
            int newDisplayType = value.toInt();
            int currentDisplayType = configStorage.getDisplayType();
            if (newDisplayType != currentDisplayType) {
                LOG_INFO_F("[WebAPI] Display type changed: %d -> %d (restart required)\n", currentDisplayType, newDisplayType);
                configStorage.setDisplayType(newDisplayType);
                displayTypeChanged = true;  // Flag for restart prompt
            }
        }
        
        // Home Assistant REST Control settings
        else if (name == "ha_base_url") configStorage.setHABaseUrl(value);
        else if (name == "ha_access_token") {
            // Only update token if not empty (allows saving other settings without re-entering token)
            if (!value.isEmpty()) {
                LOG_DEBUG("[WebAPI] HA Access Token updated (value hidden for security)");
                configStorage.setHAAccessToken(value);
            }
        }
        else if (name == "ha_light_sensor_entity") configStorage.setHALightSensorEntity(value);
        else if (name == "light_sensor_min_lux") configStorage.setLightSensorMinLux(value.toFloat());
        else if (name == "light_sensor_max_lux") configStorage.setLightSensorMaxLux(value.toFloat());
        else if (name == "display_min_brightness") configStorage.setDisplayMinBrightness(value.toInt());
        else if (name == "display_max_brightness") configStorage.setDisplayMaxBrightness(value.toInt());
        else if (name == "ha_poll_interval") configStorage.setHAPollInterval(value.toInt());
        else if (name == "light_sensor_mapping_mode") configStorage.setLightSensorMappingMode(value.toInt());        // Time settings
        else if (name == "ntp_server") configStorage.setNTPServer(value);
        else if (name == "timezone") configStorage.setTimezone(value);
    }
    
    // Handle checkbox parameters - HTML forms only send checked checkbox values
    // If a checkbox parameter is not present, it means the checkbox was unchecked
    // However, we need to distinguish between a full form submission and a partial update
    // (e.g., when JavaScript sends only brightness_auto_mode without other checkboxes)
    
    // Check which checkboxes are explicitly mentioned in the request (checked or with _present suffix)
    bool hasCyclingEnabled = server->hasArg("cycling_enabled") || server->hasArg("cycling_enabled_present");
    bool hasRandomOrder = server->hasArg("random_order") || server->hasArg("random_order_present");
    bool hasBrightnessAutoMode = server->hasArg("brightness_auto_mode") || server->hasArg("brightness_auto_mode_present");
    bool hasHADiscovery = server->hasArg("ha_discovery_enabled") || server->hasArg("ha_discovery_enabled_present");
    bool hasNTPEnabled = server->hasArg("ntp_enabled") || server->hasArg("ntp_enabled_present");
    bool hasUseHARestControl = server->hasArg("use_ha_rest_control") || server->hasArg("use_ha_rest_control_present");
    
    // Process brightness mode changes first to handle mutual exclusion properly
    if (hasBrightnessAutoMode) {
        bool enableMQTTBrightness = server->hasArg("brightness_auto_mode");
        configStorage.setBrightnessAutoMode(enableMQTTBrightness);
        
        if (enableMQTTBrightness) {
            LOG_INFO("[WebAPI] MQTT brightness control enabled - auto-disabling HA REST Control");
            configStorage.setUseHARestControl(false);
        }
    }
    
    // Logic: If enabling HA REST control, automatically disable MQTT auto mode to prevent conflicts
    if (hasUseHARestControl) {
        bool enableHARestControl = server->hasArg("use_ha_rest_control");
        configStorage.setUseHARestControl(enableHARestControl);
        
        if (enableHARestControl) {
            LOG_INFO("[WebAPI] HA REST Control enabled - auto-disabling MQTT brightness control");
            configStorage.setBrightnessAutoMode(false);
        }
    }
    
    // Only update checkboxes that are explicitly present in this request
    bool wasCycling = configStorage.getCyclingEnabled();
    bool nowCycling = wasCycling;  // Default to current value
    bool modeChanged = false;
    
    if (hasCyclingEnabled) {
        nowCycling = server->hasArg("cycling_enabled");
        modeChanged = (wasCycling != nowCycling);
        
        if (modeChanged) {
            LOG_INFO_F("[WebAPI] Cycling mode changed: %s -> %s\n", 
                       wasCycling ? "enabled" : "disabled", 
                       nowCycling ? "enabled" : "disabled");
        }
        configStorage.setCyclingEnabled(nowCycling);
    }
    
    if (hasRandomOrder) {
        configStorage.setRandomOrder(server->hasArg("random_order"));
    }
    
    if (hasHADiscovery) {
        configStorage.setHADiscoveryEnabled(server->hasArg("ha_discovery_enabled"));
    }
    
    if (hasNTPEnabled) {
        configStorage.setNTPEnabled(server->hasArg("ntp_enabled"));
    }
    
    // Save configuration to persistent storage
    configStorage.saveConfig();
    
    // Re-sync time if NTP settings changed
    if (server->hasArg("ntp_server") || server->hasArg("timezone") || server->hasArg("ntp_enabled")) {
        extern WiFiManager wifiManager;
        wifiManager.syncNTPTime();
    }
    
    // Reload configuration in the running system
    reloadConfiguration();
    
    // Apply immediate changes
    if (brightnessChanged && newBrightness >= 0) {
        displayManager.setBrightness(newBrightness);
    }
    
    if (imageSettingsChanged) {
        applyImageSettings();
    }
    
    // Handle display type change - requires restart
    if (displayTypeChanged) {
        LOG_WARNING("[WebAPI] Display type changed - restart required for display hardware reinitialization");
        needsRestart = true;
    }
    
    // Switch images immediately when mode changes
    if (modeChanged) {
        extern void advanceToNextImage();
        extern void downloadAndDisplayImage();
        extern unsigned long lastUpdate;
        extern unsigned long lastCycleTime;
        extern bool cyclingEnabled;
        
        // Update the global cycling state
        cyclingEnabled = nowCycling;
        
        if (nowCycling) {
            // Switched to multi-image mode: reset to first image (index 0)
            Serial.println("[Mode] Switched to CYCLING mode (multi-image)");
            char modeMsg[64];
            snprintf(modeMsg, sizeof(modeMsg), "Mode: CYCLING (%d images)", configStorage.getImageSourceCount());
            displayManager.debugPrint(modeMsg, COLOR_CYAN);
            configStorage.setCurrentImageIndex(0);
            configStorage.saveConfig();
            lastCycleTime = millis();
        } else {
            // Switched to single-image mode: load the single image URL
            Serial.println("[Mode] Switched to SINGLE IMAGE mode");
            displayManager.debugPrint("Mode: SINGLE IMAGE", COLOR_CYAN);
        }
        
        // Force immediate download
        lastUpdate = 0;
        downloadAndDisplayImage();
    }
    
    // Consolidate restart requirement
    if (displayTypeChanged) {
        needsRestart = true;
    }
    
    // Prepare response message
    String message = "Configuration saved successfully";
    if (needsRestart) message += " (restart required for changes to take effect)";
    if (brightnessChanged) message += " - brightness applied immediately";
    if (imageSettingsChanged) message += " - image settings applied immediately";
    
    LOG_INFO_F("[WebAPI] Configuration save completed: %s\n", message.c_str());
    
    String response = "{\"status\":\"success\",\"message\":\"" + message + "\",\"needsRestart\":" + (needsRestart ? "true" : "false") + "}";
    sendResponse(200, "application/json", response);
}

void WebConfig::handleRestart() {
    LOG_WARNING("[WebAPI] Device restart requested via web interface");
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Device restarting now...\"}");
    
    displayManager.debugPrint("Device restart requested...", COLOR_YELLOW);
    
    delay(500);
    crashLogger.saveBeforeReboot();
    ESP.restart();
}

void WebConfig::handleAddImageSource() {
    if (server->hasArg("url")) {
        String url = server->arg("url");
        if (url.length() > 0) {
            LOG_INFO_F("[WebAPI] Adding image source: %s\n", url.c_str());
            configStorage.addImageSource(url);
            configStorage.saveConfig();
            LOG_INFO_F("[WebAPI] Image source added successfully (total: %d)\n", configStorage.getImageSourceCount());
            sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source added successfully\"}");
        } else {
            LOG_WARNING("[WebAPI] Attempted to add empty image source URL");
            sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid URL\"}");
        }
    } else {
        LOG_WARNING("[WebAPI] Add image source called without URL parameter");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"URL parameter required\"}");
    }
}

void WebConfig::handleRemoveImageSource() {
    if (server->hasArg("index")) {
        int index = server->arg("index").toInt();
        LOG_INFO_F("[WebAPI] Remove image source request - index=%d\n", index);
        
        if (configStorage.removeImageSource(index)) {
            configStorage.saveConfig();
            LOG_INFO_F("[WebAPI] Image source removed successfully (remaining: %d)\n", configStorage.getImageSourceCount());
            sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source removed successfully\"}");
        } else {
            LOG_WARNING_F("[WebAPI] Failed to remove image source at index %d (invalid or last source)\n", index);
            sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to remove source: invalid index or last source\"}");
        }
    } else {
        LOG_WARNING("[WebAPI] Remove image source called without index parameter");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
    }
}

void WebConfig::handleUpdateImageSource() {
    if (server->hasArg("index") && server->hasArg("url")) {
        int index = server->arg("index").toInt();
        String url = server->arg("url");
        LOG_INFO_F("[WebAPI] Updating image source %d to: %s\n", index, url.c_str());
        configStorage.setImageSource(index, url);
        configStorage.saveConfig();
        LOG_DEBUG_F("[WebAPI] Image source %d updated successfully\n", index);
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Image source updated successfully\"}");
    } else {
        LOG_WARNING("[WebAPI] Update image source called with missing parameters");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index and URL parameters required\"}");
    }
}

void WebConfig::handleClearImageSources() {
    configStorage.clearImageSources();
    configStorage.addImageSource(configStorage.getImageURL());
    configStorage.saveConfig();
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"All image sources cleared, reset to single default source\"}");
}

void WebConfig::handleBulkDeleteImageSources() {
    if (!server->hasArg("indices")) {
        LOG_WARNING("[WebAPI] Bulk delete called without indices parameter");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Indices parameter required\"}");
        return;
    }
    
    String indicesJson = server->arg("indices");
    LOG_INFO_F("[WebAPI] Bulk delete request - indices=%s\n", indicesJson.c_str());
    
    // Parse JSON array manually (simple parsing for array of numbers)
    indicesJson.trim();
    if (!indicesJson.startsWith("[") || !indicesJson.endsWith("]")) {
        LOG_WARNING("[WebAPI] Invalid JSON format for indices");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid indices format\"}");
        return;
    }
    
    // Extract indices and sort in reverse order (delete from end to preserve indices)
    int indices[MAX_IMAGE_SOURCES];
    int count = 0;
    indicesJson = indicesJson.substring(1, indicesJson.length() - 1); // Remove brackets
    
    int startPos = 0;
    while (startPos < indicesJson.length() && count < MAX_IMAGE_SOURCES) {
        int commaPos = indicesJson.indexOf(',', startPos);
        String numStr;
        if (commaPos == -1) {
            numStr = indicesJson.substring(startPos);
            numStr.trim();
            if (numStr.length() > 0) {
                indices[count++] = numStr.toInt();
            }
            break;
        } else {
            numStr = indicesJson.substring(startPos, commaPos);
            numStr.trim();
            if (numStr.length() > 0) {
                indices[count++] = numStr.toInt();
            }
            startPos = commaPos + 1;
        }
    }
    
    if (count == 0) {
        LOG_WARNING("[WebAPI] No valid indices parsed");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"No valid indices provided\"}");
        return;
    }
    
    // Check if trying to delete all sources
    if (count >= configStorage.getImageSourceCount()) {
        LOG_WARNING("[WebAPI] Attempted to delete all sources");
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Cannot delete all sources. At least one must remain.\"}");
        return;
    }
    
    // Sort indices in descending order to delete from end
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (indices[i] < indices[j]) {
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }
        }
    }
    
    // Delete each source
    int successCount = 0;
    for (int i = 0; i < count; i++) {
        if (configStorage.removeImageSource(indices[i])) {
            successCount++;
            LOG_DEBUG_F("[WebAPI] Deleted source at index %d\n", indices[i]);
        } else {
            LOG_WARNING_F("[WebAPI] Failed to delete source at index %d\n", indices[i]);
        }
    }
    
    if (successCount > 0) {
        configStorage.saveConfig();
        String message = String("{\"status\":\"success\",\"message\":\"Successfully deleted ") + 
                        String(successCount) + String(" of ") + String(count) + String(" source(s)\",\"deleted\":") + 
                        String(successCount) + String(",\"remaining\":") + 
                        String(configStorage.getImageSourceCount()) + String("}");
        LOG_INFO_F("[WebAPI] Bulk delete completed - %d sources deleted, %d remaining\n", 
                   successCount, configStorage.getImageSourceCount());
        sendResponse(200, "application/json", message);
    } else {
        LOG_ERROR("[WebAPI] Bulk delete failed - no sources were deleted");
        sendResponse(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to delete any sources\"}");
    }
}

void WebConfig::handleNextImage() {
    extern void advanceToNextImage();
    extern void updateCyclingVariables();
    extern void downloadAndDisplayImage();
    extern unsigned long lastUpdate;
    extern unsigned long lastCycleTime;
    
    LOG_INFO("[WebAPI] Next image requested via web interface");
    updateCyclingVariables();
    advanceToNextImage();
    lastCycleTime = millis(); // Reset cycle timer for fresh interval
    lastUpdate = 0; // Force immediate image download
    downloadAndDisplayImage();
    LOG_DEBUG("[WebAPI] Image advance completed");
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Switched to next image and refreshed display\"}");
}

void WebConfig::handleForceRefresh() {
    extern void downloadAndDisplayImage();
    extern unsigned long lastUpdate;
    
    LOG_INFO("[WebAPI] Force refresh requested via web interface - redownloading current image");
    lastUpdate = 0; // Force immediate image download
    downloadAndDisplayImage();
    LOG_DEBUG("[WebAPI] Image refresh completed");
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Current image refreshed\"}");
}

void WebConfig::handleUpdateImageTransform() {
    if (server->hasArg("index") && server->hasArg("property") && server->hasArg("value")) {
        int index = server->arg("index").toInt();
        String property = server->arg("property");
        String value = server->arg("value");
        
        // Pause cycling when user is actively editing transforms
        extern bool cyclingPausedForEditing;
        extern unsigned long lastEditActivity;
        cyclingPausedForEditing = true;
        lastEditActivity = millis();
        
        LOG_DEBUG_F("[WebAPI] Transform update: image %d, %s = %s\n", index, property.c_str(), value.c_str());
        
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
        
        // Keep editing session active when applying transforms
        extern bool cyclingPausedForEditing;
        extern unsigned long lastEditActivity;
        cyclingPausedForEditing = true;
        lastEditActivity = millis();
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

void WebConfig::handleToggleImageEnabled() {
    if (!server->hasArg("index")) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
        return;
    }
    
    int index = server->arg("index").toInt();
    
    if (index < 0 || index >= configStorage.getImageSourceCount()) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
        return;
    }
    
    // Toggle the enabled state
    bool currentState = configStorage.isImageEnabled(index);
    bool newState = !currentState;
    int currentImageIndex = configStorage.getCurrentImageIndex();
    
    configStorage.setImageEnabled(index, newState);
    configStorage.saveConfig();
    
    LOG_INFO_F("[WebAPI] Image #%d %s\n", index + 1, newState ? "enabled" : "disabled");
    LOG_INFO_F("[WebAPI] Current image index: %d, toggling index: %d\n", currentImageIndex, index);
    
    // Handle automatic image switching
    bool shouldSwitchImage = false;
    
    if (!newState && index == currentImageIndex) {
        // Disabling the currently displayed image - switch to next enabled image
        LOG_INFO("[WebAPI] Currently displayed image disabled, switching to next image");
        extern void advanceToNextImage();
        advanceToNextImage();
        shouldSwitchImage = true;
    } else if (newState && index != currentImageIndex) {
        // Enabling a different image - switch to it immediately
        LOG_INFO_F("[WebAPI] Switching to newly enabled image #%d\n", index + 1);
        LOG_INFO_F("[WebAPI] Setting current index from %d to %d\n", currentImageIndex, index);
        configStorage.setCurrentImageIndex(index);
        configStorage.saveConfig();
        
        // Update the global currentImageIndex variable used by getCurrentImageURL()
        extern int currentImageIndex;
        currentImageIndex = index;
        
        extern void updateCurrentImageTransformSettings();
        updateCurrentImageTransformSettings();
        shouldSwitchImage = true;
        LOG_INFO("[WebAPI] Image switch flag set, will download new image");
    }
    
    // Trigger image download and display if we switched
    if (shouldSwitchImage) {
        LOG_INFO("[WebAPI] Triggering downloadAndDisplayImage()");
        extern void downloadAndDisplayImage();
        downloadAndDisplayImage();
    }
    
    String response = "{\"status\":\"success\",\"enabled\":" + String(newState ? "true" : "false") + 
                      ",\"switched\":" + String(shouldSwitchImage ? "true" : "false") + "}";
    sendResponse(200, "application/json", response);
}

void WebConfig::handleSelectImage() {
    if (!server->hasArg("index")) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
        return;
    }
    
    int index = server->arg("index").toInt();
    
    if (index < 0 || index >= configStorage.getImageSourceCount()) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
        return;
    }
    
    LOG_INFO_F("[WebAPI] Selecting image #%d for editing\n", index + 1);
    
    // Pause automatic cycling when user selects an image for editing
    extern bool cyclingPausedForEditing;
    extern unsigned long lastEditActivity;
    cyclingPausedForEditing = true;
    lastEditActivity = millis();
    LOG_INFO("[WebAPI] Cycling paused for editing (will resume after 30s of inactivity)");
    
    // Update both the global and config storage index
    configStorage.setCurrentImageIndex(index);
    configStorage.saveConfig();
    
    extern int currentImageIndex;
    currentImageIndex = index;
    
    // Update transform settings and switch to this image
    extern void updateCurrentImageTransformSettings();
    updateCurrentImageTransformSettings();
    
    extern void downloadAndDisplayImage();
    downloadAndDisplayImage();
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"index\":" + String(index) + "}");
}

void WebConfig::handleClearEditingState() {
    LOG_INFO("[WebAPI] Clearing editing state - resuming auto-cycling");
    
    // Clear the editing pause flag
    extern bool cyclingPausedForEditing;
    cyclingPausedForEditing = false;
    
    sendResponse(200, "application/json", "{\"status\":\"success\"}");
}

void WebConfig::handleUpdateImageDuration() {
    if (!server->hasArg("index") || !server->hasArg("duration")) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing parameters\"}");
        return;
    }
    
    int index = server->arg("index").toInt();
    unsigned long duration = server->arg("duration").toInt();
    
    if (index < 0 || index >= configStorage.getImageSourceCount()) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
        return;
    }
    
    if (duration < 5 || duration > 3600) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Duration must be between 5 and 3600 seconds\"}");
        return;
    }
    
    LOG_INFO_F("[WebAPI] Updating image #%d duration to %lu seconds\n", index + 1, duration);
    configStorage.setImageDuration(index, duration);
    configStorage.saveConfig();
    
    sendResponse(200, "application/json", "{\"status\":\"success\"}");
}

void WebConfig::handleFactoryReset() {
    LOG_WARNING("[WebAPI] Factory reset requested via web interface");
    configStorage.resetToDefaults();
    
    // Clear WiFi credentials and provisioning flag to trigger captive portal on next boot
    configStorage.setWiFiSSID("");
    configStorage.setWiFiPassword("");
    configStorage.setWiFiProvisioned(false);
    configStorage.saveConfig();
    
    LOG_WARNING("[WebAPI] Factory reset completed - WiFi setup will run on next boot");
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset completed. WiFi setup will run on next boot. Device restarting...\"}");
    
    displayManager.debugPrint("Factory reset in progress...", COLOR_YELLOW);
    displayManager.debugPrint("WiFi setup portal will run on restart", COLOR_CYAN);
    
    delay(500);
    crashLogger.saveBeforeReboot();
    ESP.restart();
}

void WebConfig::handleSetLogSeverity() {
    if (!server->hasArg("severity")) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing severity parameter\"}");
        return;
    }
    
    int severity = server->arg("severity").toInt();
    
    // Validate severity range (0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL)
    if (severity < 0 || severity > 4) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid severity level. Must be 0-4\"}");
        return;
    }
    
    // Update configuration
    configStorage.setMinLogSeverity(severity);
    configStorage.saveConfig();
    
    const char* severityNames[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
    LOG_INFO_F("[WebAPI] Log severity filter changed to: %s (%d)\n", severityNames[severity], severity);
    
    String json = "{";
    json += "\"status\":\"success\",";
    json += "\"message\":\"Log severity filter updated to " + String(severityNames[severity]) + "\",";
    json += "\"severity\":" + String(severity);
    json += "}";
    
    sendResponse(200, "application/json", json);
}

void WebConfig::handleClearCrashLogs() {
    // Clear crash logs from device memory
    crashLogger.clearAll();
    
    String json = "{";
    json += "\"status\":\"success\",";
    json += "\"message\":\"Crash logs cleared from RTC and NVS storage\"";
    json += "}";
    
    sendResponse(200, "application/json", json);
    
    LOG_INFO("[WebConfig] Crash logs cleared by user request");
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
    
    LOG_INFO("[WebAPI] Reloading configuration from web interface");
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
    size_t heapBefore = ESP.getFreeHeap();
    LOG_DEBUG_F("[WebAPI] /api/info request (heap before: %d bytes)\n", heapBefore);
    
    // Comprehensive device information API endpoint
    String json;
    json.reserve(8000);  // Pre-allocate ~8KB for large JSON response to prevent fragmentation
    json = "{";
    
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
    json += "\"use_ha_rest_control\":" + String(configStorage.getUseHARestControl() ? "true" : "false") + ",";
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
        json += "\"default_image_duration\":" + String(configStorage.getDefaultImageDuration()) + ",";
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
            json += "\"enabled\":" + String(configStorage.isImageEnabled(i) ? "true" : "false") + ",";
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
    json += "\"critical_psram_threshold\":" + String(configStorage.getCriticalPSRAMThreshold()) + ",";
    json += "\"display_type\":" + String(configStorage.getDisplayType());
    json += "},";
    
    // Time settings
    json += "\"time\":{";
    json += "\"ntp_enabled\":" + String(configStorage.getNTPEnabled() ? "true" : "false") + ",";
    json += "\"ntp_server\":\"" + escapeJson(configStorage.getNTPServer()) + "\",";
    json += "\"timezone\":\"" + escapeJson(configStorage.getTimezone()) + "\"";
    json += "}";
    
    json += "}";
    
    sendResponse(200, "application/json", json);
    
    size_t heapAfter = ESP.getFreeHeap();
    int heapDelta = (int)heapBefore - (int)heapAfter;
    if (heapDelta > 0) {
        LOG_WARNING_F("[WebAPI] /api/info request used %d bytes heap (before: %d, after: %d)\n", 
                      heapDelta, heapBefore, heapAfter);
    } else {
        LOG_DEBUG_F("[WebAPI] /api/info completed (heap after: %d bytes)\n", heapAfter);
    }
}

void WebConfig::handleCurrentImage() {
    // Return a simple status indicating the current image URL
    // Note: We don't return actual image data to avoid memory issues
    // Instead, return metadata that can be used to display the source
    String json = "{";
    json += "\"status\":\"success\",";
    json += "\"current_url\":\"" + escapeJson(configStorage.getCurrentImageURL()) + "\",";
    json += "\"cycling_enabled\":" + String(configStorage.getCyclingEnabled() ? "true" : "false") + ",";
    
    if (configStorage.getCyclingEnabled()) {
        json += "\"current_index\":" + String(configStorage.getCurrentImageIndex()) + ",";
        json += "\"total_sources\":" + String(configStorage.getImageSourceCount()) + ",";
    }
    
    json += "\"message\":\"Image data is displayed on the device. Use the current URL to fetch the source image.\"";
    json += "}";
    
    sendResponse(200, "application/json", json);
}

void WebConfig::handleForceBrightnessUpdate() {
    LOG_INFO("[WebAPI] Force brightness update requested");
    
    // Check current brightness mode and trigger appropriate update
    extern MQTTManager mqttManager;
    extern HARestClient haRestClient;
    extern HADiscovery haDiscovery;
    
    if (configStorage.getUseHARestControl() && haRestClient.isRunning()) {
        // Trigger immediate HA REST update
        LOG_INFO("[WebAPI] Forcing HA REST brightness update");
        int brightness = haRestClient.getLastBrightness();
        if (brightness >= 0) {
            displayManager.setBrightness(brightness);
            LOG_INFO_F("[WebAPI] Applied HA brightness: %d%%\n", brightness);
        }
    } else if (configStorage.getBrightnessAutoMode()) {
        // For MQTT mode, just publish current state since MQTT is command-driven
        LOG_INFO("[WebAPI] MQTT auto mode active - publishing current brightness state");
        haDiscovery.publishState();
    } else {
        // Manual mode - use default brightness
        int brightness = configStorage.getDefaultBrightness();
        displayManager.setBrightness(brightness);
        LOG_INFO_F("[WebAPI] Applied manual brightness: %d%%\n", brightness);
    }
    
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Brightness updated\"}");
}

void WebConfig::handleGetHealth() {
    LOG_INFO("[WebAPI] Health diagnostics requested via API");
    
    // Generate comprehensive health report
    DeviceHealthReport report = deviceHealth.generateReport();
    
    // Convert to JSON
    String json = deviceHealth.getReportJSON(report);
    
    LOG_DEBUG_F("[WebAPI] Health report generated: status=%s\n", 
                report.overallStatus == HEALTH_EXCELLENT ? "EXCELLENT" :
                report.overallStatus == HEALTH_GOOD ? "GOOD" :
                report.overallStatus == HEALTH_WARNING ? "WARNING" :
                report.overallStatus == HEALTH_CRITICAL ? "CRITICAL" : "FAILING");
    
    sendResponse(200, "application/json", json);
}
