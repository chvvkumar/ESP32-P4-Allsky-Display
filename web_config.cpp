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
    
    server = new WebServer(port);
    
    // Setup routes
    server->on("/", [this]() { handleRoot(); });
    server->on("/config/network", [this]() { handleNetworkConfig(); });
    server->on("/config/mqtt", [this]() { handleMQTTConfig(); });
    server->on("/config/image", [this]() { handleImageConfig(); });
    server->on("/config/display", [this]() { handleDisplayConfig(); });
    server->on("/config/advanced", [this]() { handleAdvancedConfig(); });
    server->on("/status", [this]() { handleStatus(); });
    server->on("/api/save", HTTP_POST, [this]() { handleSaveConfig(); });
    server->on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
    server->on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
    server->onNotFound([this]() { handleNotFound(); });
    
    server->begin();
    serverRunning = true;
    
    Serial.printf("Web configuration server started on port %d\n", port);
    return true;
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
    String html = generateHeader("AllSky Display Dashboard");
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
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Restarting device...\"}");
    delay(1000);
    ESP.restart();
}

void WebConfig::handleFactoryReset() {
    configStorage.resetToDefaults();
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Factory reset completed\"}");
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
    
    // Inline CSS for fast loading
    html += "<style>";
    html += "*{margin:0;padding:0;box-sizing:border-box}";
    html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;";
    html += "background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;color:#333}";
    html += ".header{background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);";
    html += "padding:1rem 0;box-shadow:0 2px 20px rgba(0,0,0,0.1)}";
    html += ".container{max-width:1200px;margin:0 auto;padding:0 1rem}";
    html += ".header-content{display:flex;justify-content:space-between;align-items:center;color:white}";
    html += ".logo{font-size:1.5rem;font-weight:bold}";
    html += ".status-badges{display:flex;gap:0.5rem}";
    html += ".badge{padding:0.25rem 0.75rem;border-radius:15px;font-size:0.75rem;font-weight:500}";
    html += ".badge.success{background:#10b981;color:white}";
    html += ".badge.error{background:#ef4444;color:white}";
    html += ".badge.warning{background:#f59e0b;color:white}";
    html += ".nav{background:rgba(255,255,255,0.05);padding:0.5rem 0}";
    html += ".nav-content{display:flex;gap:1rem;overflow-x:auto}";
    html += ".nav-item{padding:0.5rem 1rem;border-radius:8px;text-decoration:none;color:rgba(255,255,255,0.8);";
    html += "white-space:nowrap;transition:all 0.2s ease}";
    html += ".nav-item:hover,.nav-item.active{background:rgba(255,255,255,0.2);color:white}";
    html += ".main{padding:2rem 0}";
    html += ".card{background:white;border-radius:12px;padding:1.5rem;margin-bottom:1.5rem;";
    html += "box-shadow:0 4px 6px rgba(0,0,0,0.1);transition:transform 0.2s ease}";
    html += ".card:hover{transform:translateY(-2px)}";
    html += ".card h2{margin-bottom:1rem;color:#1f2937;display:flex;align-items:center;gap:0.5rem}";
    html += ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:1.5rem}";
    html += ".form-group{margin-bottom:1rem}";
    html += ".form-group label{display:block;margin-bottom:0.5rem;font-weight:500;color:#374151}";
    html += ".form-control{width:100%;padding:0.75rem;border:2px solid #e5e7eb;border-radius:8px;";
    html += "font-size:1rem;transition:border-color 0.2s ease}";
    html += ".form-control:focus{outline:none;border-color:#3b82f6}";
    html += ".btn{display:inline-block;padding:0.75rem 1.5rem;border:none;border-radius:8px;";
    html += "text-decoration:none;font-weight:500;cursor:pointer;transition:all 0.2s ease}";
    html += ".btn-primary{background:linear-gradient(135deg,#3b82f6,#1d4ed8);color:white}";
    html += ".btn-primary:hover{transform:translateY(-1px);box-shadow:0 4px 12px rgba(59,130,246,0.4)}";
    html += ".btn-success{background:linear-gradient(135deg,#10b981,#047857);color:white}";
    html += ".btn-danger{background:linear-gradient(135deg,#ef4444,#dc2626);color:white}";
    html += ".btn-secondary{background:#6b7280;color:white}";
    html += ".status-indicator{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:0.5rem}";
    html += ".status-online{background:#10b981}";
    html += ".status-offline{background:#ef4444}";
    html += ".status-warning{background:#f59e0b}";
    html += ".progress{background:#e5e7eb;border-radius:10px;height:20px;overflow:hidden}";
    html += ".progress-bar{background:linear-gradient(90deg,#3b82f6,#1d4ed8);height:100%;transition:width 0.3s ease}";
    html += ".stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:1rem;margin-bottom:2rem}";
    html += ".stat-card{background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);";
    html += "padding:1.5rem;border-radius:12px;text-align:center;color:white}";
    html += ".stat-value{font-size:2rem;font-weight:bold;margin-bottom:0.5rem}";
    html += ".stat-label{font-size:0.875rem;opacity:0.8}";
    html += ".footer{text-align:center;padding:2rem;color:rgba(255,255,255,0.7)}";
    html += "@media(max-width:768px){.header-content{flex-direction:column;gap:1rem}";
    html += ".nav-content{justify-content:center}.grid{grid-template-columns:1fr}}";
    html += ".error{border-left:4px solid #ef4444}.warning{border-left:4px solid #f59e0b}";
    html += ".success{border-left:4px solid #10b981}";
    html += "</style></head><body>";
    
    // Header
    html += "<div class='header'><div class='container'>";
    html += "<div class='header-content'>";
    html += "<div class='logo'>🛰️ ESP32 AllSky Display</div>";
    html += "<div class='status-badges'>";
    html += getConnectionStatus();
    html += "</div></div></div></div>";
    
    return html;
}

String WebConfig::generateNavigation(const String& currentPage) {
    String html = "<div class='nav'><div class='container'><div class='nav-content'>";
    
    String pages[] = {"dashboard", "network", "mqtt", "image", "display", "advanced"};
    String labels[] = {"🏠 Dashboard", "📡 Network", "🔗 MQTT", "🖼️ Image", "💡 Display", "⚙️ Advanced"};
    String urls[] = {"/", "/config/network", "/config/mqtt", "/config/image", "/config/display", "/config/advanced"};
    
    for (int i = 0; i < 6; i++) {
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
    html += "<div class='stat-value'>" + formatUptime(millis()) + "</div>";
    html += "<div class='stat-label'>Uptime</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreeHeap()) + "</div>";
    html += "<div class='stat-label'>Free Heap</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<div class='stat-value'>" + formatBytes(systemMonitor.getCurrentFreePsram()) + "</div>";
    html += "<div class='stat-label'>Free PSRAM</div></div>";
    
    html += "<div class='stat-card'>";
    html += "<div class='stat-value'>" + String(displayManager.getBrightness()) + "%</div>";
    html += "<div class='stat-label'>Brightness</div></div>";
    html += "</div>";
    
    // Quick status cards
    html += "<div class='grid'>";
    
    // WiFi Status
    html += "<div class='card'>";
    html += "<h2>📡 Network Status</h2>";
    if (wifiManager.isConnected()) {
        html += "<p><span class='status-indicator status-online'></span>Connected to " + String(WiFi.SSID()) + "</p>";
        html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
        html += "<p>Signal: " + String(WiFi.RSSI()) + " dBm</p>";
    } else {
        html += "<p><span class='status-indicator status-offline'></span>Not connected</p>";
    }
    html += "</div>";
    
    // MQTT Status
    html += "<div class='card'>";
    html += "<h2>🔗 MQTT Status</h2>";
    if (mqttManager.isConnected()) {
        html += "<p><span class='status-indicator status-online'></span>Connected to broker</p>";
        html += "<p>Server: " + configStorage.getMQTTServer() + ":" + String(configStorage.getMQTTPort()) + "</p>";
    } else {
        html += "<p><span class='status-indicator status-offline'></span>Not connected</p>";
    }
    html += "</div>";
    
    // Image Status
    html += "<div class='card'>";
    html += "<h2>🖼️ Image Status</h2>";
    html += "<p>Source: " + configStorage.getImageURL() + "</p>";
    html += "<p>Update Interval: " + String(configStorage.getUpdateInterval() / 1000 / 60) + " minutes</p>";
    html += "</div>";
    
    html += "</div>";
    
    // Quick actions
    html += "<div class='card'>";
    html += "<h2>⚡ Quick Actions</h2>";
    html += "<div style='display:flex;gap:1rem;flex-wrap:wrap'>";
    html += "<button class='btn btn-primary' onclick='restart()'>🔄 Restart Device</button>";
    html += "<button class='btn btn-danger' onclick='factoryReset()'>🏭 Factory Reset</button>";
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
    html += "<div class='card'>";
    html += "<button type='submit' class='btn btn-primary'>💾 Save MQTT Settings</button>";
    html += "</div></form></div></div>";
    
    return html;
}

String WebConfig::generateImagePage() {
    String html = "<div class='main'><div class='container'>";
    html += "<form id='imageForm'><div class='grid'>";
    
    // Image source
    html += "<div class='card'>";
    html += "<h2>🖼️ Image Source</h2>";
    
    html += "<div class='form-group'>";
    html += "<label for='image_url'>Image URL</label>";
    html += "<input type='url' id='image_url' name='image_url' class='form-control' value='" + 
            escapeHtml(configStorage.getImageURL()) + "' required>";
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
    html += "<div class='card'>";
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
    html += "<div style='text-align:center;margin-top:0.5rem'><span id='brightnessValue'>" + 
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
    html += "<div class='card'>";
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
    html += "<label for='update_interval'>Image Update Interval (minutes)</label>";
    html += "<input type='number' id='update_interval' name='update_interval' class='form-control' value='" + 
            String(configStorage.getUpdateInterval() / 1000 / 60) + "' min='1' max='1440'>";
    html += "</div>";
    
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
    html += "<div class='card'>";
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
    // Form submission handlers
    html += "function submitForm(formId,endpoint){";
    html += "const form=document.getElementById(formId);";
    html += "if(!form)return;";
    html += "form.addEventListener('submit',function(e){";
    html += "e.preventDefault();";
    html += "const formData=new FormData(form);";
    html += "fetch(endpoint,{method:'POST',body:formData})";
    html += ".then(response=>response.json())";
    html += ".then(data=>{";
    html += "if(data.status==='success'){";
    html += "alert('Settings saved successfully!');";
    html += "}else{";
    html += "alert('Error: '+data.message);";
    html += "}";
    html += "}).catch(error=>{";
    html += "alert('Error saving settings: '+error.message);";
    html += "});";
    html += "});";
    html += "}";
    
    // Initialize form handlers
    html += "document.addEventListener('DOMContentLoaded',function(){";
    html += "submitForm('networkForm','/api/save');";
    html += "submitForm('mqttForm','/api/save');";
    html += "submitForm('imageForm','/api/save');";
    html += "submitForm('displayForm','/api/save');";
    html += "submitForm('advancedForm','/api/save');";
    html += "});";
    
    // Brightness slider update
    html += "function updateBrightnessValue(value){";
    html += "document.getElementById('brightnessValue').textContent=value;";
    html += "}";
    
    // Restart function
    html += "function restart(){";
    html += "if(confirm('Are you sure you want to restart the device?')){";
    html += "fetch('/api/restart',{method:'POST'})";
    html += ".then(()=>{";
    html += "alert('Device is restarting...');";
    html += "setTimeout(()=>location.reload(),10000);";
    html += "});";
    html += "}";
    html += "}";
    
    // Factory reset function
    html += "function factoryReset(){";
    html += "if(confirm('Are you sure you want to reset to factory defaults? This cannot be undone!')){";
    html += "if(confirm('This will erase all your settings. Are you absolutely sure?')){";
    html += "fetch('/api/factory-reset',{method:'POST'})";
    html += ".then(()=>{";
    html += "alert('Factory reset completed. Device will restart...');";
    html += "setTimeout(()=>location.reload(),5000);";
    html += "});";
    html += "}";
    html += "}";
    html += "}";
    
    html += "</script>";
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
