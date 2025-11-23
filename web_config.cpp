#include "web_config.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// Global instance
WebConfig webConfig;

WebConfig::WebConfig() : server(nullptr), serverRunning(false) {
}

bool WebConfig::begin(int port) {
    if (serverRunning) return true;
    
    try {
        Serial.printf("Creating WebServer on port %d...\n", port);
        server = new WebServer(port);
        
        if (!server) {
            Serial.println("ERROR: Failed to allocate WebServer memory!");
            return false;
        }
        
        Serial.println("Setting up routes...");
        
        // Setup routes
        server->on("/", [this]() { handleRoot(); });
        server->on("/config/network", [this]() { handleNetworkConfig(); });
        server->on("/config/mqtt", [this]() { handleMQTTConfig(); });
        server->on("/config/image", [this]() { handleImageConfig(); });
        server->on("/config/sources", [this]() { handleImageSources(); });
        server->on("/config/display", [this]() { handleDisplayConfig(); });
        server->on("/config/advanced", [this]() { handleAdvancedConfig(); });
        server->on("/status", [this]() { handleStatus(); });
        server->on("/api/save", HTTP_POST, [this]() { handleSaveConfig(); });
        server->on("/api/add-source", HTTP_POST, [this]() { handleAddImageSource(); });
        server->on("/api/remove-source", HTTP_POST, [this]() { handleRemoveImageSource(); });
        server->on("/api/update-source", HTTP_POST, [this]() { handleUpdateImageSource(); });
        server->on("/api/clear-sources", HTTP_POST, [this]() { handleClearImageSources(); });
        server->on("/api/next-image", HTTP_POST, [this]() { handleNextImage(); });
        server->on("/api/update-transform", HTTP_POST, [this]() { handleUpdateImageTransform(); });
        server->on("/api/copy-defaults", HTTP_POST, [this]() { handleCopyDefaultsToImage(); });
        server->on("/api/apply-transform", HTTP_POST, [this]() { handleApplyTransform(); });
        server->on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
        server->on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
        server->onNotFound([this]() { handleNotFound(); });
        
        Serial.println("Starting WebServer...");
        server->begin();
        
        // Verify server is running
        if (!server) {
            Serial.println("ERROR: WebServer failed to start!");
            return false;
        }
        
        serverRunning = true;
        Serial.printf("✓ Web configuration server started successfully on port %d\n", port);
        return true;
        
    } catch (const std::exception& e) {
        Serial.printf("ERROR: Exception starting web server: %s\n", e.what());
        return false;
    } catch (...) {
        Serial.println("ERROR: Unknown exception starting web server!");
        return false;
    }
}

void WebConfig::handleClient() {
    if (serverRunning && server) {
        server->handleClient();
    }
}

bool WebConfig::isRunning() {
    return serverRunning;
}

void WebConfig::stop() {
    if (serverRunning && server) {
        server->stop();
        delete server;
        server = nullptr;
        serverRunning = false;
    }
}

void WebConfig::handleRoot() {
    String html = generateHeader("ESP32 AllSky Display");
    html += generateNavigation("dashboard");
    html += generateMainPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleNetworkConfig() {
    String html = generateHeader("Network Configuration");
    html += generateNavigation("network");
    html += generateNetworkPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleMQTTConfig() {
    String html = generateHeader("MQTT Configuration");
    html += generateNavigation("mqtt");
    html += generateMQTTPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleImageConfig() {
    String html = generateHeader("Image Configuration");
    html += generateNavigation("image");
    html += generateImagePage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleImageSources() {
    String html = generateHeader("Image Sources");
    html += generateNavigation("sources");
    html += generateImageSourcesPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleDisplayConfig() {
    String html = generateHeader("Display Configuration");
    html += generateNavigation("display");
    html += generateDisplayPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleAdvancedConfig() {
    String html = generateHeader("Advanced Configuration");
    html += generateNavigation("advanced");
    html += generateAdvancedPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleStatus() {
    String json = getSystemStatus();
    sendResponse(200, "application/json", json);
}

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
        else if (name == "mqtt_reboot_topic") configStorage.setMQTTRebootTopic(value);
        else if (name == "mqtt_brightness_topic") configStorage.setMQTTBrightnessTopic(value);
        else if (name == "mqtt_brightness_status_topic") configStorage.setMQTTBrightnessStatusTopic(value);
        
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
            unsigned long interval = value.toInt() * 1000UL; // Convert seconds to milliseconds
            configStorage.setCycleInterval(interval);
        }
        
        // Advanced settings - FIX: Convert minutes to milliseconds correctly
        else if (name == "update_interval") {
            unsigned long newInterval = value.toInt() * 60UL * 1000UL; // Convert minutes to ms correctly
            configStorage.setUpdateInterval(newInterval);
        }
        else if (name == "mqtt_reconnect_interval") configStorage.setMQTTReconnectInterval(value.toInt() * 1000);
        else if (name == "watchdog_timeout") configStorage.setWatchdogTimeout(value.toInt() * 1000);
        else if (name == "critical_heap_threshold") configStorage.setCriticalHeapThreshold(value.toInt());
        else if (name == "critical_psram_threshold") configStorage.setCriticalPSRAMThreshold(value.toInt());
    }
    
    // Handle checkbox parameters (checkboxes only send values when checked)
    // Check if cycling_enabled checkbox was submitted
    bool cyclingEnabledFound = false;
    bool randomOrderFound = false;
    
    for (int i = 0; i < server->args(); i++) {
        String name = server->argName(i);
        if (name == "cycling_enabled") {
            cyclingEnabledFound = true;
            configStorage.setCyclingEnabled(true);
        }
        else if (name == "random_order") {
            randomOrderFound = true;
            configStorage.setRandomOrder(true);
        }
    }
    
    // Check if brightness_auto_mode checkbox was submitted
    bool brightnessAutoModeFound = false;
    
    for (int i = 0; i < server->args(); i++) {
        String name = server->argName(i);
        if (name == "brightness_auto_mode") {
            brightnessAutoModeFound = true;
            configStorage.setBrightnessAutoMode(true);
        }
    }
    
    // If checkbox parameters weren't found, they are unchecked
    if (!cyclingEnabledFound) {
        configStorage.setCyclingEnabled(false);
    }
    if (!randomOrderFound) {
        configStorage.setRandomOrder(false);
    }
    if (!brightnessAutoModeFound) {
        configStorage.setBrightnessAutoMode(false);
    }
    
    // Save configuration to persistent storage
    configStorage.saveConfig();
    
    // Reload configuration in the running system
    reloadConfiguration();
    
    // Apply immediate changes
    if (brightnessChanged && newBrightness >= 0) {
        displayManager.setBrightness(newBrightness);
        Serial.printf("Applied brightness change immediately: %d%%\n", newBrightness);
        
        // Publish new brightness status via MQTT if connected
        if (mqttManager.isConnected()) {
            mqttManager.publishBrightnessStatus(newBrightness);
        }
    }
    
    // Apply image transformation changes immediately
    if (imageSettingsChanged) {
        Serial.println("Image settings changed - applying immediately");
        applyImageSettings();
    }
    
    // Prepare response message
    String message = "Configuration saved successfully";
    if (needsRestart) {
        message += " (restart required for network/MQTT changes)";
    }
    if (brightnessChanged) {
        message += " - brightness applied immediately";
    }
    if (imageSettingsChanged) {
        message += " - image settings applied immediately";
    }
    
    String response = "{\"status\":\"success\",\"message\":\"" + message + "\"}";
    sendResponse(200, "application/json", response);
}

void WebConfig::handleRestart() {
    // Send success response to browser immediately
    Serial.println("Device restart requested via web interface");
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Device restarting now...\"}");
    
    // Display message on screen
    displayManager.debugPrint("Device restart requested...", COLOR_YELLOW);
    displayManager.debugPrint("Restarting device...", COLOR_CYAN);
    displayManager.debugPrint("See you soon!", COLOR_YELLOW);
    
    // Wait briefly to ensure response is sent to browser
    delay(500);
    
    // Now perform the actual restart
    Serial.println("Executing ESP.restart()...");
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
    // Add back the default/legacy image URL as the first source
    configStorage.addImageSource(configStorage.getImageURL());
    configStorage.saveConfig();
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"All image sources cleared, reset to single default source\"}");
}

void WebConfig::handleNextImage() {
    // Trigger next image switch by calling the cycling function
    extern void advanceToNextImage();
    extern void updateCyclingVariables();
    extern void downloadAndDisplayImage();
    
    updateCyclingVariables();
    advanceToNextImage();
    
    // Force immediate image download and display
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
        
        // Apply the correct property
        if (property == "scaleX") {
            configStorage.setImageScaleX(index, value.toFloat());
        } 
        else if (property == "scaleY") {
            configStorage.setImageScaleY(index, value.toFloat());
        } 
        else if (property == "offsetX") {
            configStorage.setImageOffsetX(index, value.toInt());
        } 
        else if (property == "offsetY") {
            configStorage.setImageOffsetY(index, value.toInt());
        } 
        else if (property == "rotation") {
            configStorage.setImageRotation(index, value.toFloat());
        } 
        else {
            success = false;
            message = "Invalid property name";
        }
        
        if (success) {
            configStorage.saveConfig();
            
            // If this is the current image, apply the changes immediately
            if (index == configStorage.getCurrentImageIndex()) {
                // Apply the transform if this is the current image
                extern float scaleX, scaleY;
                extern int16_t offsetX, offsetY;
                extern float rotationAngle;
                extern void renderFullImage();
                
                // Update only the modified property
                if (property == "scaleX") {
                    scaleX = configStorage.getImageScaleX(index);
                } 
                else if (property == "scaleY") {
                    scaleY = configStorage.getImageScaleY(index);
                } 
                else if (property == "offsetX") {
                    offsetX = configStorage.getImageOffsetX(index);
                } 
                else if (property == "offsetY") {
                    offsetY = configStorage.getImageOffsetY(index);
                } 
                else if (property == "rotation") {
                    rotationAngle = configStorage.getImageRotation(index);
                }
                
                // Re-render the current image with new settings
                renderFullImage();
                Serial.printf("Applied transform property %s=%.2f to current image\n", 
                            property.c_str(), value.toFloat());
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
        
        // If this is the current image, apply the changes immediately
        if (index == configStorage.getCurrentImageIndex()) {
            // Apply the transform if this is the current image
            extern float scaleX, scaleY;
            extern int16_t offsetX, offsetY;
            extern float rotationAngle;
            extern void renderFullImage();
            
            // Update all properties from the stored transform
            scaleX = configStorage.getImageScaleX(index);
            scaleY = configStorage.getImageScaleY(index);
            offsetX = configStorage.getImageOffsetX(index);
            offsetY = configStorage.getImageOffsetY(index);
            rotationAngle = configStorage.getImageRotation(index);
            
            // Re-render the current image with new settings
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
        
        // If the requested transform is not for the current image, switch to it
        if (index != currentIndex) {
            configStorage.setCurrentImageIndex(index);
            configStorage.saveConfig();
            
            // Trigger image change
            extern void downloadAndDisplayImage();
            downloadAndDisplayImage();
            
            Serial.printf("Switched to image %d and applied its transform\n", index);
        } else {
            // Apply transform for current image
            extern float scaleX, scaleY;
            extern int16_t offsetX, offsetY;
            extern float rotationAngle;
            extern void renderFullImage();
            
            // Update all properties from the stored transform
            scaleX = configStorage.getImageScaleX(index);
            scaleY = configStorage.getImageScaleY(index);
            offsetX = configStorage.getImageOffsetX(index);
            offsetY = configStorage.getImageOffsetY(index);
            rotationAngle = configStorage.getImageRotation(index);
            
            // Re-render the current image with new settings
            renderFullImage();
            Serial.printf("Applied transform settings for image %d\n", index);
        }
        
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Transform applied successfully\"}");
    } else {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Index parameter required\"}");
    }
}

void WebConfig::handleFactoryReset() {
    // Reset configuration to factory defaults
    Serial.println("Factory reset requested via web interface");
    configStorage.resetToDefaults();
    
    // Send response before restarting
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset completed. Device restarting...\"}");
    
    // Display message on screen
    displayManager.debugPrint("Factory reset in progress...", COLOR_YELLOW);
    displayManager.debugPrint("Clearing all settings", COLOR_CYAN);
    displayManager.debugPrint("Restarting device...", COLOR_YELLOW);
    
    // Wait briefly to ensure response is sent to browser
    delay(500);
    
    // Now restart the device with factory defaults
    Serial.println("Executing ESP.restart() after factory reset...");
    ESP.restart();
}

void WebConfig::handleNotFound() {
    String html = generateHeader("Page Not Found");
    html += "<div class='container'><div class='card error'>";
    html += "<h2>🚫 Page Not Found</h2>";
    html += "<p>The requested page could not be found.</p>";
    html += "<a href='/' class='btn btn-primary'>Return to Dashboard</a>";
    html += "</div></div>";
    html += generateFooter();
    sendResponse(404, "text/html", html);
}

String WebConfig::generateHeader(const String& title) {
    String html = "<!DOCTYPE html><html lang='en'><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>" + title + "</title>";
    
    // Inline CSS for fast loading - Dark Slate Material Design Theme
    html += "<style>";
    // Load FontAwesome and Roboto
    html += "@import url('https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css');";
    html += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');";
    
    // Reset & Base Styles
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:'Roboto',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;";
    html += "background-color:#0f172a;color:#f8fafc;min-height:100vh;line-height:1.6;display:flex;flex-direction:column}"; // Slate 900 background
    
    // Modal Styles
    html += ".modal{display:none;position:fixed;z-index:1000;left:0;top:0;width:100%;height:100%;background-color:rgba(15,23,42,0.8);backdrop-filter:blur(4px)}";
    html += ".modal.show{display:flex;align-items:center;justify-content:center;animation:fadeIn 0.2s ease}";
    html += "@keyframes fadeIn{from{opacity:0}to{opacity:1}}";
    html += ".modal-content{background:#1e293b;border:1px solid #334155;border-radius:16px;padding:2rem;max-width:500px;box-shadow:0 25px 50px -12px rgba(0,0,0,0.5);animation:slideUp 0.3s ease}"; // Slate 800 card
    html += "@keyframes slideUp{from{transform:translateY(20px);opacity:0}to{transform:translateY(0);opacity:1}}";
    html += ".modal-header{display:flex;align-items:center;gap:1rem;margin-bottom:1.5rem;color:#f1f5f9;border-bottom:1px solid #334155;padding-bottom:1rem}";
    html += ".modal-title{font-size:1.5rem;font-weight:bold;flex:1}";
    html += ".modal-close{background:none;border:none;color:#94a3b8;font-size:1.5rem;cursor:pointer;padding:0;width:2rem;height:2rem;display:flex;align-items:center;justify-content:center;border-radius:8px;transition:all 0.2s}";
    html += ".modal-close:hover{background:rgba(239,68,68,0.1);color:#f8fafc}";
    html += ".modal-body{margin-bottom:1.5rem;color:#cbd5e1;line-height:1.6}";
    html += ".modal-footer{display:flex;gap:1rem;justify-content:flex-end}";
    
    // Modal Buttons
    html += ".modal-btn{padding:0.75rem 1.5rem;border:none;border-radius:8px;font-weight:500;cursor:pointer;transition:all 0.2s;font-size:0.95rem;letter-spacing:0.5px}";
    html += ".modal-btn-confirm{background:#0ea5e9;color:white}"; // Sky 500
    html += ".modal-btn-confirm:hover{background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.4)}";
    html += ".modal-btn-cancel{background:#475569;color:white}"; // Slate 600
    html += ".modal-btn-cancel:hover{background:#334155}";
    html += ".modal-success{border-left:4px solid #10b981}";
    html += ".modal-error{border-left:4px solid #ef4444}";
    html += ".modal-warning{border-left:4px solid #f59e0b}";

    // Header Styles
    html += ".header{background:#1e293b;padding:1rem 0;box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);border-bottom:1px solid #334155}";
    html += ".container{max-width:1200px;margin:0 auto;padding:0 1rem}";
    html += ".header-content{display:flex;justify-content:space-between;align-items:center;color:#f8fafc}";
    html += ".logo{font-size:1.5rem;font-weight:bold;color:#38bdf8;letter-spacing:-0.5px}"; // Sky 400
    html += ".status-badges{display:flex;gap:0.5rem}";
    html += ".badge{padding:0.35rem 0.85rem;border-radius:9999px;font-size:0.75rem;font-weight:600;letter-spacing:0.5px;text-transform:uppercase}";
    html += ".badge.success{background:#059669;color:#ecfdf5}";
    html += ".badge.error{background:#dc2626;color:#fef2f2}";
    html += ".badge.warning{background:#d97706;color:#fffbeb}";
    
    // Github Link
    html += ".github-link{display:flex;align-items:center;margin-right:1rem;padding:0.4rem 0.8rem;background:#334155;color:#e2e8f0;border-radius:8px;text-decoration:none;font-size:0.9rem;border:1px solid #475569;transition:all 0.2s ease}";
    html += ".github-link:hover{background:#475569;border-color:#64748b;transform:translateY(-1px)}";
    html += ".github-link .github-icon{font-family:'Font Awesome 6 Brands';margin-right:0.4rem}";
    
    // Navigation
    html += ".nav{background:#0f172a;padding:0;border-bottom:1px solid #334155;position:sticky;top:0;z-index:100;backdrop-filter:blur(8px);background:rgba(15,23,42,0.95)}";
    html += ".nav-content{display:flex;gap:0.5rem;overflow-x:auto;padding:0.5rem 0}";
    html += ".nav-item{padding:0.75rem 1.25rem;border-radius:8px;text-decoration:none;color:#94a3b8;white-space:nowrap;transition:all 0.2s ease;font-weight:500;font-size:0.95rem}";
    html += ".nav-item:hover{background:#1e293b;color:#38bdf8}";
    html += ".nav-item.active{background:#1e293b;color:#38bdf8;box-shadow:inset 0 -2px 0 #38bdf8}";
    
    // Main Layout
    html += ".main{padding:2rem 0;flex:1}";
    html += ".card{background:#1e293b;border:1px solid #334155;border-radius:12px;padding:1.5rem;margin-bottom:1.5rem;";
    html += "box-shadow:0 4px 6px -1px rgba(0,0,0,0.3);transition:transform 0.2s ease,box-shadow 0.2s ease;display:flex;flex-direction:column;height:100%}";
    html += ".card:hover{transform:translateY(-2px);box-shadow:0 10px 15px -3px rgba(0,0,0,0.4);border-color:#475569}";
    html += ".card h2{margin-bottom:1.25rem;color:#f8fafc;display:flex;align-items:center;gap:0.75rem;font-weight:600;font-size:1.25rem;border-bottom:1px solid #334155;padding-bottom:0.75rem}";
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1.5rem}";
    
    // Forms
    html += ".form-group{margin-bottom:1.25rem}";
    html += ".form-group label{display:block;margin-bottom:0.5rem;font-weight:500;color:#cbd5e1;font-size:0.9rem}";
    html += ".form-control{width:100%;padding:0.75rem;border:1px solid #475569;border-radius:8px;";
    html += "font-size:1rem;background:#334155;color:#f8fafc;transition:border-color 0.2s ease,box-shadow 0.2s ease}";
    html += ".form-control:focus{outline:none;border-color:#38bdf8;box-shadow:0 0 0 3px rgba(56,189,248,0.2);background:#1e293b}";
    html += ".form-control::placeholder{color:#64748b}";
    
    // Buttons
    html += ".btn{display:inline-flex;align-items:center;justify-content:center;padding:0.75rem 1.5rem;border:none;border-radius:8px;";
    html += "text-decoration:none;font-weight:500;cursor:pointer;transition:all 0.2s ease;font-size:0.95rem;letter-spacing:0.3px}";
    html += ".btn-primary{background:#0ea5e9;color:white}"; // Sky 500
    html += ".btn-primary:hover{background:#0284c7;transform:translateY(-1px);box-shadow:0 4px 12px rgba(14,165,233,0.3)}";
    html += ".btn-success{background:#10b981;color:white}"; // Emerald 500
    html += ".btn-success:hover{background:#059669;transform:translateY(-1px);box-shadow:0 4px 12px rgba(16,185,129,0.3)}";
    html += ".btn-danger{background:#ef4444;color:white}"; // Red 500
    html += ".btn-danger:hover{background:#dc2626;transform:translateY(-1px);box-shadow:0 4px 12px rgba(239,68,68,0.3)}";
    html += ".btn-secondary{background:#475569;color:white}"; // Slate 600
    html += ".btn-secondary:hover{background:#334155;transform:translateY(-1px)}";
    
    // Status Indicators - Pulsing
    html += ".status-indicator{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:0.75rem}";
    html += "@keyframes pulse-green{0%{box-shadow:0 0 0 0 rgba(16,185,129,0.7)}70%{box-shadow:0 0 0 6px rgba(16,185,129,0)}100%{box-shadow:0 0 0 0 rgba(16,185,129,0)}}";
    html += ".status-online{background:#10b981;animation:pulse-green 2s infinite}";
    html += ".status-offline{background:#ef4444;box-shadow:0 0 10px rgba(239,68,68,0.5)}";
    html += ".status-warning{background:#f59e0b;box-shadow:0 0 10px rgba(245,158,11,0.5)}";
    
    // Progress Bars
    html += ".progress{background:#334155;border-radius:9999px;height:12px;overflow:hidden;border:none}";
    html += ".progress-bar{background:linear-gradient(90deg,#38bdf8,#0ea5e9);height:100%;transition:width 0.3s ease}";
    
    // Stats Grid
    html += ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;margin-bottom:2rem}";
    html += ".stat-card{background:#1e293b;padding:1.5rem;border-radius:12px;text-align:center;color:#f8fafc;border:1px solid #334155;transition:transform 0.2s ease;position:relative;overflow:hidden}";
    html += ".stat-card:hover{transform:translateY(-2px);border-color:#475569;box-shadow:0 10px 15px -3px rgba(0,0,0,0.3)}";
    html += ".stat-value{font-size:2.25rem;font-weight:700;margin-bottom:0.25rem;color:#38bdf8;letter-spacing:-1px;position:relative;z-index:2}";
    html += ".stat-label{font-size:0.875rem;font-weight:500;color:#94a3b8;text-transform:uppercase;letter-spacing:0.5px;position:relative;z-index:2}";
    html += ".stat-icon{position:absolute;right:10px;bottom:0px;font-size:4rem;opacity:0.05;color:#f8fafc;z-index:1;transform:rotate(-15deg)}";
    
    // Misc
    html += ".footer{text-align:center;padding:2rem 1rem;color:#64748b;border-top:1px solid #334155;margin-top:auto;font-size:0.9rem}";
    html += "@media(max-width:768px){.header-content{flex-direction:column;gap:1rem;text-align:center}";
    html += ".nav-content{justify-content:flex-start}.grid{grid-template-columns:1fr}}";
    html += ".error{border-left:4px solid #ef4444}.warning{border-left:4px solid #f59e0b}";
    html += ".success{border-left:4px solid #10b981}";
    
    // Image Sources specific
    html += ".image-source-item{background:#0f172a !important; border:1px solid #334155 !important; padding:1.25rem !important;}";
    html += ".transform-section{background:#1e293b !important; border:1px dashed #475569 !important;}";

    html += "</style></head><body>";
    
    // Header
    html += "<div class='header'><div class='container'>";
    html += "<div class='header-content'>";
    html += "<div class='logo'><i class='fas fa-satellite'></i> ESP32 AllSky Display</div>";
    html += "<div class='status-badges'>";
    html += "<a href='https://github.com/chvvkumar/ESP32-P4-Allsky-Display' target='_blank' class='github-link'><i class='github-icon fa-github'></i> GitHub</a>";
    html += getConnectionStatus();
    html += "</div></div></div></div>";
    
    return html;
}

String WebConfig::generateNavigation(const String& currentPage) {
    String html = "<div class='nav'><div class='container'><div class='nav-content'>";
    
    String pages[] = {"dashboard", "network", "mqtt", "image", "sources", "display", "advanced"};
    String labels[] = {"🏠 Dashboard", "📡 Network", "🔗 MQTT", "🖼️ Single Image", "🔄 Multi-Image", "💡 Display", "⚙️ Advanced"};
    String urls[] = {"/", "/config/network", "/config/mqtt", "/config/image", "/config/sources", "/config/display", "/config/advanced"};
    
    for (int i = 0; i < 7; i++) {
        String activeClass = (currentPage == pages[i]) ? " active" : "";
        html += "<a href='" + urls[i] + "' class='nav-item" + activeClass + "'>" + labels[i] + "</a>";
    }
    
    html += "</div></div></div>";
    
    return html;
}

String WebConfig::generateMainPage() {
    String html = "<div class='main'><div class='container'>";
    
    // System status cards
    html += "<div class='stats'>";
    html += "<div class='stat-card'>";
    html += "<i class='fas fa-clock stat-icon'></i>";
    html += "<div class='stat-value'>" + formatUptime(millis()) + "</div>";
    html += "<div class='stat-label'>Uptime</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<i class='fas fa-microchip stat-icon'></i>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreeHeap()) + "</div>";
    html += "<div class='stat-label'>Free Heap</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<i class='fas fa-memory stat-icon'></i>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreePsram()) + "</div>";
    html += "<div class='stat-label'>Free PSRAM</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<i class='fas fa-sun stat-icon'></i>";
    html += "<div class='stat-value'>" + String(displayManager.getBrightness()) + "%</div>";
    html += "<div class='stat-label'>Brightness</div></div>";
    html += "</div>";
    
    // Brightness control card
    html += "<div class='card'>";
    html += "<h2>💡 Screen Brightness</h2>";
    
    // Auto/Manual toggle
    html += "<div class='form-group'>";
    html += "<label>Control Mode</label>";
    html += "<div style='margin-top:0.5rem;display:flex;align-items:center'>";
    html += "<input type='checkbox' id='brightness_auto_mode' name='brightness_auto_mode' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px'";
    if (configStorage.getBrightnessAutoMode()) {
        html += " checked";
    }
    html += " onchange='updateBrightnessMode(this.checked)'> ";
    html += "<label for='brightness_auto_mode' style='margin-bottom:0;cursor:pointer'>Auto (MQTT controlled)</label>";
    html += "</div>";
    html += "</div>";
    
    // Brightness slider
    html += "<div class='form-group' id='brightness_slider_container' style='";
    if (configStorage.getBrightnessAutoMode()) {
        html += "opacity:0.5;";
    }
    html += "'>";
    html += "<label for='main_brightness'>Brightness (%)</label>";
    html += "<input type='range' id='main_brightness' name='default_brightness' class='form-control' style='height:6px;padding:0' value='" + 
            String(displayManager.getBrightness()) + "' min='0' max='100' oninput='updateMainBrightnessValue(this.value)'";
    if (configStorage.getBrightnessAutoMode()) {
        html += " disabled";
    }
    html += ">";
    html += "<div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='mainBrightnessValue'>" + 
            String(displayManager.getBrightness()) + "</span>%</div>";
    html += "</div>";
    
    html += "<button type='button' class='btn btn-primary' onclick='saveMainBrightness(this)'";
    if (configStorage.getBrightnessAutoMode()) {
        html += " disabled";
    }
    html += " id='save_brightness_btn'>Apply Brightness</button>";
    html += "</div>";
    
    // Quick status cards
    html += "<div class='grid'>";
    
    // WiFi Status
    html += "<div class='card'>";
    html += "<h2>📡 Network Status</h2>";
    if (wifiManager.isConnected()) {
        html += "<div style='flex:1'>";
        html += "<p><span class='status-indicator status-online'></span>Connected to <strong style='color:#38bdf8'>" + String(WiFi.SSID()) + "</strong></p>";
        html += "<p style='margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'>IP Address: " + WiFi.localIP().toString() + "</p>";
        html += "<p style='font-size:0.9rem;color:#94a3b8'>Signal: " + String(WiFi.RSSI()) + " dBm</p>";
        html += "</div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // MQTT Status
    html += "<div class='card'>";
    html += "<h2>🔗 MQTT Status</h2>";
    if (mqttManager.isConnected()) {
        html += "<div style='flex:1'>";
        html += "<p><span class='status-indicator status-online'></span>Connected to broker</p>";
        html += "<p style='margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'>Server: " + configStorage.getMQTTServer() + ":" + String(configStorage.getMQTTPort()) + "</p>";
        html += "</div>";
    } else {
        html += "<div style='flex:1'><p><span class='status-indicator status-offline'></span>Not connected</p></div>";
    }
    html += "</div>";
    
    // Image Status with "Switch to Next Image" button
    html += "<div class='card'>";
    html += "<h2>🖼️ Image Status</h2>";
    html += "<div style='flex:1'>";
    
    // Check if cycling is enabled and display appropriate information
    if (configStorage.getCyclingEnabled()) {
        int sourceCount = configStorage.getImageSourceCount();
        int currentIndex = configStorage.getCurrentImageIndex();
        
        html += "<p><strong>Mode:</strong> Cycling (" + String(sourceCount) + " sources)</p>";
        html += "<p style='word-break:break-all;margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'><strong>Current Source:</strong> [" + String(currentIndex + 1) + "/" + String(sourceCount) + "] " + escapeHtml(configStorage.getCurrentImageURL()) + "</p>";
        html += "<p style='font-size:0.9rem;color:#94a3b8'><strong>Cycle Interval:</strong> " + String(configStorage.getCycleInterval() / 1000) + " seconds</p>";
    } else {
        html += "<p><strong>Mode:</strong> Single Image</p>";
        html += "<p style='word-break:break-all;margin-top:0.5rem;font-size:0.9rem;color:#94a3b8'><strong>Source:</strong> " + escapeHtml(configStorage.getImageURL()) + "</p>";
    }
    html += "</div>";
    
    html += "<p style='font-size:0.8rem;color:#64748b;margin-top:1rem'><strong>Update Interval:</strong> " + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</p>";
    html += "</div>";
    
    html += "</div>"; // End status grid
    
    // Quick actions (Full width now)
    html += "<div class='card' style='margin-top:1.5rem'>";
    html += "<h2>⚡ Quick Actions</h2>";
    html += "<div style='display:flex;gap:1rem;flex-wrap:wrap;align-items:center'>";
    
    // Image Control Button (if multiple sources)
    if (configStorage.getImageSourceCount() > 1) {
         html += "<button type='button' class='btn btn-primary' onclick='nextImage(this)'><i class='fas fa-forward' style='margin-right:8px'></i> Next Image</button>";
    }
    
    html += "<button class='btn btn-primary' style='background:#3b82f6' onclick='restart()'><i class='fas fa-sync-alt' style='margin-right:8px'></i> Restart Device</button>";
    html += "<button class='btn btn-danger' onclick='factoryReset()'><i class='fas fa-trash-alt' style='margin-right:8px'></i> Factory Reset</button>";
    html += "</div></div>";
    
    html += "</div></div>";
    
    return html;
}

String WebConfig::generateNetworkPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='networkForm'><div class='card'>";
    html += "<h2>📡 WiFi Configuration</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='wifi_ssid'>Network Name (SSID)</label>";
    html += "<input type='text' id='wifi_ssid' name='wifi_ssid' class='form-control' value='" + 
            escapeHtml(configStorage.getWiFiSSID()) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='wifi_password'>Password</label>";
    html += "<input type='password' id='wifi_password' name='wifi_password' class='form-control' value='" + 
            escapeHtml(configStorage.getWiFiPassword()) + "'>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-primary'>💾 Save Network Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateMQTTPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='mqttForm'><div class='grid'>";
    
    // Connection settings
    html += "<div class='card'>";
    html += "<h2>🔗 MQTT Broker</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_server'>Broker Address</label>";
    html += "<input type='text' id='mqtt_server' name='mqtt_server' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTServer()) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_port'>Port</label>";
    html += "<input type='number' id='mqtt_port' name='mqtt_port' class='form-control' value='" + 
            String(configStorage.getMQTTPort()) + "' min='1' max='65535' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_client_id'>Client ID</label>";
    html += "<input type='text' id='mqtt_client_id' name='mqtt_client_id' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTClientID()) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_user'>Username (optional)</label>";
    html += "<input type='text' id='mqtt_user' name='mqtt_user' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTUser()) + "'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_password'>Password (optional)</label>";
    html += "<input type='password' id='mqtt_password' name='mqtt_password' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTPassword()) + "'>";
    html += "</div>";
    html += "</div>";
    
    // Topics
    html += "<div class='card'>";
    html += "<h2>📝 MQTT Topics</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_reboot_topic'>Reboot Topic</label>";
    html += "<input type='text' id='mqtt_reboot_topic' name='mqtt_reboot_topic' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTRebootTopic()) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_brightness_topic'>Brightness Control Topic</label>";
    html += "<input type='text' id='mqtt_brightness_topic' name='mqtt_brightness_topic' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTBrightnessTopic()) + "' required>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_brightness_status_topic'>Brightness Status Topic</label>";
    html += "<input type='text' id='mqtt_brightness_status_topic' name='mqtt_brightness_status_topic' class='form-control' value='" + 
            escapeHtml(configStorage.getMQTTBrightnessStatusTopic()) + "' required>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    html += "<div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save MQTT Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateImageSourcesPage() {
    String html = "<div class='main'><div class='container'>";
    
    // Cycling Configuration
    html += "<form id='cyclingForm'><div class='card'>";
    html += "<h2>🔄 Image Cycling Configuration</h2>";
    
    html += "<div class='form-group'>";
    html += "<div style='display:flex;align-items:center;margin-bottom:1rem'>";
    html += "<input type='checkbox' id='cycling_enabled' name='cycling_enabled' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px' " + 
            String(configStorage.getCyclingEnabled() ? "checked" : "") + ">";
    html += "<label for='cycling_enabled' style='margin-bottom:0;cursor:pointer;font-size:1rem'>Enable automatic cycling through multiple image sources</label>";
    html += "</div>";
    html += "</div>";
    
    html += "<div class='grid' style='margin-bottom:1rem'>";
    html += "<div class='form-group'>";
    html += "<label for='cycle_interval'>Cycle Interval (seconds)</label>";
    html += "<input type='number' id='cycle_interval' name='cycle_interval' class='form-control' value='" + 
            String(configStorage.getCycleInterval() / 1000) + "' min='10' max='3600'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<div style='display:flex;align-items:center;height:100%'>";
    html += "<input type='checkbox' id='random_order' name='random_order' style='width:20px;height:20px;accent-color:#0ea5e9;margin-right:10px' " + 
            String(configStorage.getRandomOrder() ? "checked" : "") + ">";
    html += "<label for='random_order' style='margin-bottom:0;cursor:pointer'>Randomize image order</label>";
    html += "</div></div>";
    html += "</div>";
    
    html += "<button type='submit' class='btn btn-primary'>💾 Save Cycling Settings</button>";
    html += "</div></form>";
    
    // Image Sources List with Transformation Settings
    html += "<div class='card'>";
    html += "<h2>📋 Image Sources & Transformations</h2>";
    html += "<div id='imageSourcesList'>";
    
    int sourceCount = configStorage.getImageSourceCount();
    for (int i = 0; i < sourceCount; i++) {
        String url = configStorage.getImageSource(i);
        
        // Start image source item
        html += "<div class='image-source-item' style='margin-bottom:1.5rem;padding:1rem;border:1px solid #334155;border-radius:8px;background:#0f172a;'>";
        
        // URL input and controls
        html += "<div style='display:flex;align-items:center;gap:1rem;margin-bottom:1rem;flex-wrap:wrap'>";
        html += "<span style='font-weight:bold;color:#38bdf8;font-size:1.1rem'>" + String(i + 1) + ".</span>";
        html += "<input type='url' class='form-control' style='flex:1;min-width:200px' value='" + escapeHtml(url) + "' onchange='updateImageSource(" + String(i) + ", this)'>";
        html += "<button type='button' class='btn btn-secondary' onclick='toggleTransformSection(" + String(i) + ")'><i class='fas fa-cog'></i> Options</button>";
        if (sourceCount > 1) {
            html += "<button type='button' class='btn btn-danger' onclick='removeImageSource(" + String(i) + ", this)'><i class='fas fa-trash'></i></button>";
        }
        html += "</div>";
        
        // Transformation settings section (initially hidden)
        html += "<div id='transformSection_" + String(i) + "' class='transform-section' style='display:none;margin-top:1rem;padding:1.5rem;background:#1e293b;border-radius:8px;border:1px solid #475569;'>";
        html += "<h3 style='margin-top:0;font-size:1rem;color:#cbd5e1;margin-bottom:1rem;border-bottom:1px solid #334155;padding-bottom:0.5rem'>Transform Settings</h3>";
        
        // Transformation controls
        html += "<div style='display:grid;grid-template-columns:repeat(auto-fit, minmax(150px, 1fr));gap:1rem;'>";
        
        // Scale X
        html += "<div class='form-group'>";
        html += "<label>Scale X</label>";
        html += "<input type='number' class='form-control' value='" + 
                String(configStorage.getImageScaleX(i)) + "' step='0.1' min='0.1' max='5.0' onchange='updateImageTransform(" + String(i) + ", \"scaleX\", this)'>";
        html += "</div>";
        
        // Scale Y
        html += "<div class='form-group'>";
        html += "<label>Scale Y</label>";
        html += "<input type='number' class='form-control' value='" + 
                String(configStorage.getImageScaleY(i)) + "' step='0.1' min='0.1' max='5.0' onchange='updateImageTransform(" + String(i) + ", \"scaleY\", this)'>";
        html += "</div>";
        
        // Offset X
        html += "<div class='form-group'>";
        html += "<label>Offset X</label>";
        html += "<input type='number' class='form-control' value='" + 
                String(configStorage.getImageOffsetX(i)) + "' onchange='updateImageTransform(" + String(i) + ", \"offsetX\", this)'>";
        html += "</div>";
        
        // Offset Y
        html += "<div class='form-group'>";
        html += "<label>Offset Y</label>";
        html += "<input type='number' class='form-control' value='" + 
                String(configStorage.getImageOffsetY(i)) + "' onchange='updateImageTransform(" + String(i) + ", \"offsetY\", this)'>";
        html += "</div>";
        
        // Rotation
        html += "<div class='form-group'>";
        html += "<label>Rotation</label>";
        html += "<select class='form-control' onchange='updateImageTransform(" + String(i) + ", \"rotation\", this)'>";
        int rotation = (int)configStorage.getImageRotation(i);
        html += String("<option value='0'") + (rotation == 0 ? " selected" : "") + ">0°</option>";
        html += String("<option value='90'") + (rotation == 90 ? " selected" : "") + ">90°</option>";
        html += String("<option value='180'") + (rotation == 180 ? " selected" : "") + ">180°</option>";
        html += String("<option value='270'") + (rotation == 270 ? " selected" : "") + ">270°</option>";
        html += "</select>";
        html += "</div>";
        
        html += "</div>"; // End grid layout
        
        // Action buttons for transformations
        html += "<div style='margin-top:1rem;display:flex;gap:0.5rem;'>";
        html += "<button type='button' class='btn btn-secondary' onclick='copyDefaultsToImage(" + String(i) + ", this)'>Copy Defaults</button>";
        html += "<button type='button' class='btn btn-secondary' onclick='applyTransformImmediately(" + String(i) + ", this)'>Apply Now</button>";
        html += "</div>";
        
        html += "</div>"; // End transform section
        html += "</div>"; // End image source item
    }
    
    html += "</div>"; // End imageSourcesList
    
    // Image source action buttons
    html += "<div style='margin-top:1rem;'>";
    html += "<button type='button' class='btn btn-success' onclick='addImageSource(this)'>➕ Add Image Source</button>";
    if (sourceCount > 1) {
        html += "<button type='button' class='btn btn-secondary' onclick='clearAllSources(this)' style='margin-left:1rem;'>🗑️ Clear All</button>";
    }
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
}

String WebConfig::generateImagePage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='imageForm'><div class='grid'>";
    
    // Image source
    html += "<div class='card'>";
    html += "<h2>🖼️ Image Source</h2>";
    html += "<p style='color:#f59e0b;margin-bottom:1rem;background:rgba(245,158,11,0.1);padding:0.75rem;border-radius:6px;border-left:4px solid #f59e0b'>⚠️ For multiple image sources, use the <a href='/config/sources' style='color:#38bdf8;font-weight:bold'>Image Sources</a> page.</p>";
    
    html += "<div class='form-group'>";
    html += "<label for='image_url'>Image URL</label>";
    html += "<input type='url' id='image_url' name='image_url' class='form-control' value='" + 
            escapeHtml(configStorage.getImageURL()) + "' required>";
    html += "</div>";
    
    // Add update interval control (moved from Advanced page)
    html += "<div class='form-group'>";
    html += "<label for='update_interval'>Image Update Interval (minutes)</label>";
    html += "<input type='number' id='update_interval' name='update_interval' class='form-control' value='" + 
            String(configStorage.getUpdateInterval() / 1000 / 60) + "' min='1' max='1440'>";
    html += "</div>";
    
    html += "</div>";
    
    // Transform defaults
    html += "<div class='card'>";
    html += "<h2>🔄 Default Transformations</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_scale_x'>Scale X</label>";
    html += "<input type='number' id='default_scale_x' name='default_scale_x' class='form-control' value='" + 
            String(configStorage.getDefaultScaleX()) + "' step='0.1' min='0.1' max='5.0'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_scale_y'>Scale Y</label>";
    html += "<input type='number' id='default_scale_y' name='default_scale_y' class='form-control' value='" + 
            String(configStorage.getDefaultScaleY()) + "' step='0.1' min='0.1' max='5.0'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_offset_x'>Offset X (pixels)</label>";
    html += "<input type='number' id='default_offset_x' name='default_offset_x' class='form-control' value='" + 
            String(configStorage.getDefaultOffsetX()) + "'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_offset_y'>Offset Y (pixels)</label>";
    html += "<input type='number' id='default_offset_y' name='default_offset_y' class='form-control' value='" + 
            String(configStorage.getDefaultOffsetY()) + "'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_rotation'>Rotation (degrees)</label>";
    html += "<input type='number' id='default_rotation' name='default_rotation' class='form-control' value='" + 
            String(configStorage.getDefaultRotation()) + "' step='90' min='0' max='270'>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    html += "<div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Image Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateDisplayPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='displayForm'><div class='grid'>";
    
    // Brightness settings
    html += "<div class='card'>";
    html += "<h2>💡 Brightness Control</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='default_brightness'>Default Brightness (%)</label>";
    html += "<input type='range' id='default_brightness' name='default_brightness' class='form-control' value='" + 
            String(configStorage.getDefaultBrightness()) + "' min='0' max='100' oninput='updateBrightnessValue(this.value)'>";
    html += "<div style='text-align:center;margin-top:0.5rem;color:#38bdf8;font-weight:bold'><span id='brightnessValue'>" + 
            String(configStorage.getDefaultBrightness()) + "</span>%</div>";
    html += "</div>";
    html += "</div>";
    
    // Backlight settings
    html += "<div class='card'>";
    html += "<h2>⚙️ Backlight Settings</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='backlight_freq'>PWM Frequency (Hz)</label>";
    html += "<input type='number' id='backlight_freq' name='backlight_freq' class='form-control' value='" + 
            String(configStorage.getBacklightFreq()) + "' min='1000' max='20000'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='backlight_resolution'>PWM Resolution (bits)</label>";
    html += "<input type='number' id='backlight_resolution' name='backlight_resolution' class='form-control' value='" + 
            String(configStorage.getBacklightResolution()) + "' min='8' max='16'>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    html += "<div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Display Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateAdvancedPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='advancedForm'><div class='grid'>";
    
    // Timing settings
    html += "<div class='card'>";
    html += "<h2>⏱️ Timing Settings</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='mqtt_reconnect_interval'>MQTT Reconnect Interval (seconds)</label>";
    html += "<input type='number' id='mqtt_reconnect_interval' name='mqtt_reconnect_interval' class='form-control' value='" + 
            String(configStorage.getMQTTReconnectInterval() / 1000) + "' min='1' max='300'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='watchdog_timeout'>Watchdog Timeout (seconds)</label>";
    html += "<input type='number' id='watchdog_timeout' name='watchdog_timeout' class='form-control' value='" + 
            String(configStorage.getWatchdogTimeout() / 1000) + "' min='10' max='120'>";
    html += "</div>";
    html += "</div>";
    
    // Memory settings
    html += "<div class='card'>";
    html += "<h2>💾 Memory Thresholds</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='critical_heap_threshold'>Critical Heap Threshold (bytes)</label>";
    html += "<input type='number' id='critical_heap_threshold' name='critical_heap_threshold' class='form-control' value='" + 
            String(configStorage.getCriticalHeapThreshold()) + "' min='10000' max='1000000'>";
    html += "</div>";
    
    html += "<div class='form-group'>";
    html += "<label for='critical_psram_threshold'>Critical PSRAM Threshold (bytes)</label>";
    html += "<input type='number' id='critical_psram_threshold' name='critical_psram_threshold' class='form-control' value='" + 
            String(configStorage.getCriticalPSRAMThreshold()) + "' min='10000' max='10000000'>";
    html += "</div>";
    html += "</div>";
    
    html += "</div>";
    html += "<div class='card' style='margin-top:1.5rem'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save Advanced Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateStatusPage() {
    String html = "<div class='main'><div class='container'>";
    html += "<div class='card'>";
    html += "<h2>📊 System Status</h2>";
    html += "<div id='statusData'>Loading...</div>";
    html += "</div></div></div>";
    
    return html;
}

String WebConfig::generateFooter() {
    String html = "<script>";
    
    // New feedback functions
    html += "function showButtonFeedback(btn, type, message) {";
    html += "if(!btn) return;";
    html += "const originalContent = btn.getAttribute('data-original-content') || btn.innerHTML;";
    html += "if (!btn.getAttribute('data-original-content')) {";
    html += "btn.setAttribute('data-original-content', originalContent);";
    html += "}";
    html += "const originalClass = btn.getAttribute('data-original-class') || btn.className;";
    html += "if (!btn.getAttribute('data-original-class')) {";
    html += "btn.setAttribute('data-original-class', originalClass);";
    html += "}";
    html += "if (type === 'loading') {";
    html += "btn.disabled = true;";
    html += "btn.innerHTML = '<i class=\"fas fa-circle-notch fa-spin\"></i> ' + (message || 'Working...');";
    html += "return;";
    html += "}";
    html += "btn.disabled = false;";
    html += "if (type === 'success') {";
    html += "btn.className = 'btn btn-success';";
    html += "btn.innerHTML = '<i class=\"fas fa-check\"></i> ' + (message || 'Saved');";
    html += "} else {";
    html += "btn.className = 'btn btn-danger';";
    html += "btn.innerHTML = '<i class=\"fas fa-exclamation-triangle\"></i> ' + (message || 'Error');";
    html += "}";
    html += "setTimeout(() => {";
    html += "btn.innerHTML = originalContent;";
    html += "btn.className = originalClass;";
    html += "btn.disabled = false;";
    html += "}, 2000);";
    html += "}";

    html += "function showInputFeedback(input, type) {";
    html += "const originalBorder = input.style.borderColor;";
    html += "if (type === 'success') {";
    html += "input.style.borderColor = '#10b981';";
    html += "input.style.boxShadow = '0 0 0 3px rgba(16, 185, 129, 0.2)';";
    html += "} else {";
    html += "input.style.borderColor = '#ef4444';";
    html += "input.style.boxShadow = '0 0 0 3px rgba(239, 68, 68, 0.2)';";
    html += "}";
    html += "setTimeout(() => {";
    html += "input.style.borderColor = '';";
    html += "input.style.boxShadow = '';";
    html += "}, 2000);";
    html += "}";
    
    // Modal functions
    html += "function showModal(title,message,type='info'){";
    html += "const modal=document.getElementById('modal');";
    html += "const modalTitle=document.querySelector('.modal-title');";
    html += "const modalBody=document.querySelector('.modal-body');";
    html += "const modalContent=document.querySelector('.modal-content');";
    html += "if(!modal||!modalTitle||!modalBody||!modalContent)return;";
    html += "modalTitle.innerHTML=title;";
    html += "modalBody.innerHTML=message;";
    html += "modalContent.className='modal-content';";
    html += "if(type==='success')modalContent.classList.add('modal-success');";
    html += "else if(type==='error')modalContent.classList.add('modal-error');";
    html += "else if(type==='warning')modalContent.classList.add('modal-warning');";
    html += "modal.classList.add('show');";
    html += "}";
    
    html += "function closeModal(){";
    html += "const modal=document.getElementById('modal');";
    html += "if(modal)modal.classList.remove('show');";
    html += "}";
    
    html += "function showConfirmModal(title,message,onConfirm,onCancel){";
    html += "const modal=document.getElementById('confirmModal');";
    html += "const modalTitle=document.querySelector('#confirmModal .modal-title');";
    html += "const modalBody=document.querySelector('#confirmModal .modal-body');";
    html += "const confirmBtn=document.querySelector('#confirmModal .modal-btn-confirm');";
    html += "if(!modal||!modalTitle||!modalBody||!confirmBtn)return;";
    html += "modalTitle.innerHTML=title;";
    html += "modalBody.innerHTML=message;";
    html += "modal.classList.add('show');";
    html += "confirmBtn.onclick=()=>{modal.classList.remove('show');if(onConfirm)onConfirm();};";
    html += "const cancelBtn=document.querySelector('#confirmModal .modal-btn-cancel');";
    html += "if(cancelBtn)cancelBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel();};";
    html += "const closeBtn=document.querySelector('#confirmModal .modal-close');";
    html += "if(closeBtn)closeBtn.onclick=()=>{modal.classList.remove('show');if(onCancel)onCancel();};";
    html += "}";
    
    // Modal close handlers
    html += "document.addEventListener('DOMContentLoaded',function(){";
    html += "const closeButtons=document.querySelectorAll('.modal-close');";
    html += "closeButtons.forEach(btn=>{btn.onclick=()=>closeModal();});";
    html += "});";
    
    // Form submission handlers with button feedback
    html += "function submitForm(formId,endpoint){";
    html += "const form=document.getElementById(formId);";
    html += "if(!form)return;";
    html += "form.addEventListener('submit',function(e){";
    html += "e.preventDefault();";
    html += "const btn = form.querySelector('button[type=\"submit\"]');";
    html += "if(btn) showButtonFeedback(btn, 'loading', 'Saving...');";
    html += "const formData=new FormData(form);";
    html += "fetch(endpoint,{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "if(btn) showButtonFeedback(btn, 'success', 'Saved!');";
    html += "if(data.willRestart){";
    html += "showModal('✓ Restarting', data.message, 'success');";
    html += "setTimeout(()=>location.reload(), 10000);"; // Reload for restart
    html += "}";
    html += "}else{";
    html += "if(btn) showButtonFeedback(btn, 'error', 'Error: ' + data.message);";
    // html += "showModal('✗ Error','Error: '+data.message,'error');"; // REMOVED MODAL
    html += "}";
    html += "}).catch(error=>{";
    html += "if(btn) showButtonFeedback(btn, 'error', 'Failed');";
    // html += "showModal('✗ Error','Error saving settings: '+error.message,'error');"; // REMOVED MODAL
    html += "});";
    html += "});";
    html += "}";
    
    // Initialize form handlers
    html += "document.addEventListener('DOMContentLoaded',function(){";
    html += "submitForm('networkForm','/api/save');";
    html += "submitForm('mqttForm','/api/save');";
    html += "submitForm('imageForm','/api/save');";
    html += "submitForm('cyclingForm','/api/save');";
    html += "submitForm('displayForm','/api/save');";
    html += "submitForm('advancedForm','/api/save');";
    html += "});";
    
    // Brightness slider update
    html += "function updateBrightnessValue(value){";
    html += "document.getElementById('brightnessValue').textContent=value;";
    html += "}";
    
    // Main page brightness slider update
    html += "function updateMainBrightnessValue(value){";
    html += "document.getElementById('mainBrightnessValue').textContent=value;";
    html += "}";
    
    // Brightness mode toggle
    html += "function updateBrightnessMode(isAuto){";
    html += "const slider=document.getElementById('main_brightness');";
    html += "const container=document.getElementById('brightness_slider_container');";
    html += "const saveButton=document.getElementById('save_brightness_btn');";
    html += "const checkbox=document.getElementById('brightness_auto_mode');";
    html += "if(isAuto){";
    html += "slider.disabled=true;";
    html += "saveButton.disabled=true;";
    html += "container.style.opacity='0.5';";
    html += "}else{";
    html += "slider.disabled=false;";
    html += "saveButton.disabled=false;";
    html += "container.style.opacity='1';";
    html += "}";
    
    // Save the form data and apply brightness without page reload
    html += "const formData=new FormData();";
    html += "formData.append('brightness_auto_mode',isAuto?'on':'');";
    html += "fetch('/api/save',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status!=='success'){";
    html += "showInputFeedback(checkbox, 'error');"; // REPLACED ALERT
    html += "}";
    html += "});";
    html += "}";
    
    // Save main brightness
    html += "function saveMainBrightness(btn){";
    html += "showButtonFeedback(btn, 'loading', 'Applying...');";
    html += "const value=document.getElementById('main_brightness').value;";
    html += "const formData=new FormData();";
    html += "formData.append('default_brightness',value);";
    html += "fetch('/api/save',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Applied!');";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(e=>{ showButtonFeedback(btn, 'error', 'Failed'); });";
    html += "}";
    
    // Restart function
    html += "function restart(){";
    html += "showConfirmModal('🔄 Restart Device','Are you sure you want to restart the device?',()=>{";
    html += "fetch('/api/restart',{method:'POST'})";
    html += ".then(()=>{";
    html += "showModal('✓ Restarting','Device is restarting...','success');";
    html += "setTimeout(()=>location.reload(),10000);";
    html += "});";
    html += "});";
    html += "}";
    
    // Factory reset function
    html += "function factoryReset(){";
    html += "showConfirmModal('🏭 Factory Reset','Are you sure you want to reset to factory defaults? This cannot be undone!',()=>{";
    html += "showConfirmModal('⚠️ Confirm Factory Reset','This will erase ALL your settings. Are you absolutely sure?',()=>{";
    html += "fetch('/api/factory-reset',{method:'POST'})";
    html += ".then(()=>{";
    html += "showModal('✓ Factory Reset','Device will restart now...','success');";
    html += "setTimeout(()=>location.reload(),5000);";
    html += "});";
    html += "});";
    html += "});";
    html += "}";
    
    // Image source management functions
    html += "function addImageSource(btn){";
    html += "const url=prompt('Enter image URL:');";
    html += "if(url&&url.trim()){";
    html += "showButtonFeedback(btn, 'loading', 'Adding...');";
    html += "const formData=new FormData();";
    html += "formData.append('url',url.trim());";
    html += "fetch('/api/add-source',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Added!');";
    html += "setTimeout(()=>location.reload(), 1000);";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error: ' + data.message);";
    // html += "alert('Error: '+data.message);"; // REMOVED ALERT
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "}";
    html += "}";
    
    html += "function removeImageSource(index, btn){";
    html += "showConfirmModal('🗑️ Remove Image Source','Are you sure you want to remove this image source?',()=>{";
    html += "showButtonFeedback(btn, 'loading', 'Removing...');";
    html += "const formData=new FormData();";
    html += "formData.append('index',index);";
    html += "fetch('/api/remove-source',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Removed!');";
    html += "setTimeout(()=>location.reload(),1000);";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "});";
    html += "}";
    
    html += "function updateImageSource(index, input){";
    html += "const url = input.value;";
    html += "const formData=new FormData();";
    html += "formData.append('index',index);";
    html += "formData.append('url',url);";
    html += "fetch('/api/update-source',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status!=='success'){";
    html += "showInputFeedback(input, 'error');";
    html += "}else{ showInputFeedback(input, 'success'); }";
    html += "}).catch(error=>{";
    html += "showInputFeedback(input, 'error');";
    html += "});";
    html += "}";
    
    html += "function clearAllSources(btn){";
    html += "showConfirmModal('🗑️ Clear All Sources','Are you sure you want to clear all image sources? This will reset to a single default source.',()=>{";
    html += "showButtonFeedback(btn, 'loading', 'Clearing...');";
    html += "fetch('/api/clear-sources',{method:'POST'})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Cleared!');";
    html += "setTimeout(()=>location.reload(),1000);";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "});";
    html += "}";
    
    // Per-image transform functions
    html += "function toggleTransformSection(index){";
    html += "const section=document.getElementById('transformSection_'+index);";
    html += "if(section){";
    html += "section.style.display=section.style.display==='none'?'block':'none';";
    html += "}";
    html += "}";
    
    html += "function updateImageTransform(index, property, input){";
    html += "const value = input.value;";
    html += "const formData=new FormData();";
    html += "formData.append('index',index);";
    html += "formData.append('property',property);";
    html += "formData.append('value',value);";
    html += "fetch('/api/update-transform',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status!=='success'){";
    html += "showInputFeedback(input, 'error');";
    html += "}else{ showInputFeedback(input, 'success'); }";
    html += "}).catch(error=>{";
    html += "showInputFeedback(input, 'error');";
    html += "});";
    html += "}";
    
    html += "function copyDefaultsToImage(index, btn){";
    html += "showConfirmModal('📋 Copy Global Defaults','Are you sure you want to copy global default transformation settings to this image?',()=>{";
    html += "showButtonFeedback(btn, 'loading', 'Copying...');";
    html += "const formData=new FormData();";
    html += "formData.append('index',index);";
    html += "fetch('/api/copy-defaults',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Copied!');";
    html += "setTimeout(()=>location.reload(),1000);";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "});";
    html += "}";
    
    html += "function applyTransformImmediately(index, btn){";
    html += "showConfirmModal('⚙️ Apply Transform','Apply these transformation settings immediately?',()=>{";
    html += "showButtonFeedback(btn, 'loading', 'Applying...');";
    html += "const formData=new FormData();";
    html += "formData.append('index',index);";
    html += "fetch('/api/apply-transform',{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Applied!');";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "});";
    html += "}";
    
    html += "function nextImage(btn){";
    html += "showButtonFeedback(btn, 'loading', 'Switching...');";
    html += "fetch('/api/next-image',{method:'POST'})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "showButtonFeedback(btn, 'success', 'Switched!');";
    html += "setTimeout(()=>location.reload(),2000);";
    html += "}else{";
    html += "showButtonFeedback(btn, 'error', 'Error');";
    html += "}";
    html += "}).catch(error=>{";
    html += "showButtonFeedback(btn, 'error', 'Failed');";
    html += "});";
    html += "}";
    
    html += "</script>";
    html += "<div id='modal' class='modal'><div class='modal-content'>";
    html += "<div class='modal-header'><h2 class='modal-title'></h2><button class='modal-close' onclick='closeModal()'>&times;</button></div>";
    html += "<div class='modal-body'></div>";
    html += "<div class='modal-footer'><button class='modal-btn modal-btn-confirm' onclick='closeModal()'>OK</button></div>";
    html += "</div></div>";
    html += "<div id='confirmModal' class='modal'><div class='modal-content'>";
    html += "<div class='modal-header'><h2 class='modal-title'></h2><button class='modal-close' onclick='closeModal()'>&times;</button></div>";
    html += "<div class='modal-body'></div>";
    html += "<div class='modal-footer'><button class='modal-btn modal-btn-cancel'>Cancel</button><button class='modal-btn modal-btn-confirm'>Confirm</button></div>";
    html += "</div></div>";
    html += "<div class='footer'>";
    html += "<div class='container'>";
    html += "<p>ESP32 AllSky Display Configuration Portal</p>";
    html += "</div></div>";
    html += "</body></html>";
    
    return html;
}

String WebConfig::getSystemStatus() {
    String json = "{";
    json += "\"wifi_connected\":" + String(wifiManager.isConnected() ? "true" : "false") + ",";
    json += "\"wifi_ssid\":\"" + (wifiManager.isConnected() ? String(WiFi.SSID()) : "Not connected") + "\",";
    json += "\"wifi_ip\":\"" + (wifiManager.isConnected() ? WiFi.localIP().toString() : "0.0.0.0") + "\",";
    json += "\"wifi_rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"mqtt_connected\":" + String(mqttManager.isConnected() ? "true" : "false") + ",";
    json += "\"free_heap\":" + String(systemMonitor.getCurrentFreeHeap()) + ",";
    json += "\"free_psram\":" + String(systemMonitor.getCurrentFreePsram()) + ",";
    json += "\"uptime\":" + String(millis()) + ",";
    json += "\"brightness\":" + String(displayManager.getBrightness());
    json += "}";
    return json;
}

String WebConfig::formatUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    
    if (days > 0) {
        return String(days) + "d " + String(hours % 24) + "h";
    } else if (hours > 0) {
        return String(hours) + "h " + String(minutes % 60) + "m";
    } else if (minutes > 0) {
        return String(minutes) + "m " + String(seconds % 60) + "s";
    } else {
        return String(seconds) + "s";
    }
}

String WebConfig::formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + "KB";
    } else {
        return String(bytes / (1024.0 * 1024.0), 1) + "MB";
    }
}

String WebConfig::getConnectionStatus() {
    String html = "";
    if (wifiManager.isConnected()) {
        html += "<span class='badge success'>WiFi ✓</span>";
    } else {
        html += "<span class='badge error'>WiFi ✗</span>";
    }
    
    if (mqttManager.isConnected()) {
        html += "<span class='badge success'>MQTT ✓</span>";
    } else {
        html += "<span class='badge error'>MQTT ✗</span>";
    }
    
    if (systemMonitor.isSystemHealthy()) {
        html += "<span class='badge success'>System ✓</span>";
    } else {
        html += "<span class='badge warning'>System ⚠</span>";
    }
    
    return html;
}

String WebConfig::escapeHtml(const String& input) {
    String output = input;
    output.replace("&", "&amp;");
    output.replace("<", "&lt;");
    output.replace(">", "&gt;");
    output.replace("\"", "&quot;");
    output.replace("'", "&#x27;");
    return output;
}

void WebConfig::sendResponse(int code, const String& contentType, const String& content) {
    if (server) {
        server->send(code, contentType, content);
    }
}

void WebConfig::applyImageSettings() {
    // Apply stored configuration values to the running system
    // Note: These global variables are declared in the main .ino file
    extern float scaleX, scaleY;
    extern int16_t offsetX, offsetY;
    extern float rotationAngle;
    extern void renderFullImage();
    
    // Update the global transformation variables
    scaleX = configStorage.getDefaultScaleX();
    scaleY = configStorage.getDefaultScaleY();
    offsetX = configStorage.getDefaultOffsetX();
    offsetY = configStorage.getDefaultOffsetY();
    rotationAngle = configStorage.getDefaultRotation();
    
    Serial.printf("Applied image settings: Scale=%.1fx%.1f, Offset=%d,%d, Rotation=%.0f°\n", 
                  scaleX, scaleY, offsetX, offsetY, rotationAngle);
    
    // Re-render the current image with new settings
    renderFullImage();
}

void WebConfig::reloadConfiguration() {
    // Reload cycling configuration and notify main system
    extern void updateCyclingVariables();
    
    Serial.println("Reloading configuration from web interface...");
    
    // Update cycling variables in the main system
    updateCyclingVariables();
    
    // Force the main system to pick up new configuration
    extern unsigned long currentUpdateInterval;
    extern unsigned long currentCycleInterval;
    extern bool cyclingEnabled;
    extern bool randomOrderEnabled;
    extern int imageSourceCount;
    
    // Update runtime variables immediately
    currentUpdateInterval = configStorage.getUpdateInterval();
    currentCycleInterval = configStorage.getCycleInterval();
    cyclingEnabled = configStorage.getCyclingEnabled();
    randomOrderEnabled = configStorage.getRandomOrder();
    imageSourceCount = configStorage.getImageSourceCount();
    
    Serial.printf("Configuration reloaded: cycling=%s, interval=%lu ms, sources=%d\n",
                  cyclingEnabled ? "enabled" : "disabled", currentCycleInterval, imageSourceCount);
}