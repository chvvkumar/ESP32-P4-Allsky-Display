#include "web_config.h"
#include "web_config_html.h"
#include "build_info.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"

// Global instance
WebConfig webConfig;

WebConfig::WebConfig() : server(nullptr), serverRunning(false) {}

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
        server->on("/config/commands", [this]() { handleSerialCommands(); });
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
        server->on("/api/info", HTTP_GET, [this]() { handleGetAllInfo(); });
        server->on("/api/current-image", HTTP_GET, [this]() { handleCurrentImage(); });
        
        // Initialize ElegantOTA
        ElegantOTA.begin(server);
        ElegantOTA.onStart([]() {
            Serial.println("ElegantOTA: Update started");
            displayManager.debugPrint("OTA Update starting...", COLOR_YELLOW);
            displayManager.pauseDisplay();
            systemMonitor.forceResetWatchdog();
        });
        ElegantOTA.onProgress([](size_t current, size_t final) {
            // Reset watchdog on every progress update to prevent timeout
            systemMonitor.forceResetWatchdog();
            
            static uint8_t lastPercent = 0;
            uint8_t percent = (current * 100) / final;
            if (percent != lastPercent && percent % 10 == 0) {
                Serial.printf("ElegantOTA Progress: %u%%\n", percent);
                char msg[64];
                snprintf(msg, sizeof(msg), "OTA Progress: %u%%", percent);
                displayManager.debugPrint(msg, COLOR_CYAN);
                lastPercent = percent;
            }
        });
        ElegantOTA.onEnd([](bool success) {
            systemMonitor.forceResetWatchdog();
            displayManager.resumeDisplay();
            if (success) {
                Serial.println("ElegantOTA: Update successful!");
                displayManager.debugPrint("OTA Complete! Rebooting...", COLOR_GREEN);
            } else {
                Serial.println("ElegantOTA: Update failed!");
                displayManager.debugPrint("OTA Update Failed", COLOR_RED);
            }
        });
        server->on("/api-reference", [this]() { handleAPIReference(); });
        server->onNotFound([this]() { handleNotFound(); });
        
        Serial.println("Starting WebServer...");
        server->begin();
        
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
    if (server && serverRunning) {
        server->handleClient();
        ElegantOTA.loop();
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

// Route handlers - these call the page generators from web_config_pages.cpp
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

void WebConfig::handleSerialCommands() {
    String html = generateHeader("Serial Commands");
    html += generateNavigation("commands");
    html += generateSerialCommandsPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleStatus() {
    String json = getSystemStatus();
    sendResponse(200, "application/json", json);
}

void WebConfig::handleAPIReference() {
    String html = generateHeader("API Reference");
    html += generateNavigation("api");
    html += generateAPIReferencePage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
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

// HTML template generation functions
String WebConfig::generateHeader(const String& title) {
    String html = "<!DOCTYPE html><html lang='en'><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>" + title + "</title>";
    html += "<style>" + String(FPSTR(HTML_CSS)) + "</style></head><body>";
    
    // Header
    html += "<div class='header'><div class='container'>";
    html += "<div class='header-content'>";
    html += "<div class='logo'><i class='fas fa-satellite'></i> ESP32 AllSky Display</div>";
    html += "<div class='status-badges'>";
    html += "<a href='https://github.com/chvvkumar/ESP32-P4-Allsky-Display' target='_blank' class='github-link'><i class='github-icon fa-github'></i> GitHub</a>";
    html += getConnectionStatus();
    if (configStorage.getImageSourceCount() > 1) {
        html += "<button type='button' class='github-link' style='cursor:pointer;border:none' onclick='nextImage(this)'><i class='fas fa-forward' style='margin-right:6px'></i> Next</button>";
    }
    html += "<button class='github-link' style='cursor:pointer;border:none;background:#3b82f6;border-color:#2563eb' onclick='restart()'><i class='fas fa-sync-alt' style='margin-right:6px'></i> Restart</button>";
    html += "<button class='github-link' style='cursor:pointer;border:none;background:#ef4444;border-color:#dc2626' onclick='factoryReset()'><i class='fas fa-trash-alt' style='margin-right:6px'></i> Reset</button>";
    html += "</div></div></div></div>";
    
    return html;
}

String WebConfig::generateNavigation(const String& currentPage) {
    String html = "<div class='nav'><div class='container' style='position:relative'>";
    html += "<button class='nav-toggle' onclick='toggleNav()' aria-label='Toggle navigation'><i class='fas fa-bars'></i></button>";
    html += "<div class='nav-content'>";
    
    String pages[] = {"dashboard", "network", "mqtt", "image", "sources", "display", "advanced", "commands", "api"};
    String labels[] = {"🏠 Dashboard", "📡 Network", "🔗 MQTT", "🖼️ Single Image", "🔄 Multi-Image", "💡 Display", "⚙️ Advanced", "📟 Commands", "📚 API"};
    String urls[] = {"/", "/config/network", "/config/mqtt", "/config/image", "/config/sources", "/config/display", "/config/advanced", "/config/commands", "/api-reference"};
    
    for (int i = 0; i < 9; i++) {
        String activeClass = (currentPage == pages[i]) ? " active" : "";
        html += "<a href='" + urls[i] + "' class='nav-item" + activeClass + "'>" + labels[i] + "</a>";
    }
    
    html += "</div></div></div>";
    return html;
}

String WebConfig::generateFooter() {
    String html = "<script>" + String(FPSTR(HTML_JAVASCRIPT)) + "</script>";
    html += String(FPSTR(HTML_MODALS));
    
    html += "<div class='footer'><div class='container'>";
    html += "<p style='margin-bottom:0.5rem'>ESP32 AllSky Display Configuration Portal</p>";
    html += "<p style='font-size:0.8rem;color:#64748b;margin:0.25rem 0'>";
    html += "MD5: " + String(ESP.getSketchMD5().substring(0, 8)) + " | ";
    html += "Build: " + formatBytes(ESP.getSketchSize()) + " | ";
    html += "Free: " + formatBytes(ESP.getFreeSketchSpace());
    html += "</p>";
    html += "<p style='font-size:0.75rem;color:#475569;margin:0.25rem 0'>";
    html += "Built: " + String(BUILD_DATE) + " " + String(BUILD_TIME) + " | ";
    html += "Commit: <span style='font-family:monospace'>" + String(GIT_COMMIT_HASH) + "</span> (" + String(GIT_BRANCH) + ")";
    html += "</p></div></div>";
    html += "</body></html>";
    return html;
}

// Utility functions
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
    
    if (days > 0) return String(days) + "d " + String(hours % 24) + "h";
    else if (hours > 0) return String(hours) + "h " + String(minutes % 60) + "m";
    else if (minutes > 0) return String(minutes) + "m " + String(seconds % 60) + "s";
    else return String(seconds) + "s";
}

String WebConfig::formatBytes(size_t bytes) {
    if (bytes < 1024) return String(bytes) + "B";
    else if (bytes < 1024 * 1024) return String(bytes / 1024.0, 1) + "KB";
    else return String(bytes / (1024.0 * 1024.0), 1) + "MB";
}

String WebConfig::getConnectionStatus() {
    String html = "";
    if (wifiManager.isConnected()) html += "<span class='badge success'>WiFi ✓</span>";
    else html += "<span class='badge error'>WiFi ✗</span>";
    
    if (mqttManager.isConnected()) html += "<span class='badge success'>MQTT ✓</span>";
    else html += "<span class='badge error'>MQTT ✗</span>";
    
    if (systemMonitor.isSystemHealthy()) html += "<span class='badge success'>System ✓</span>";
    else html += "<span class='badge warning'>System ⚠</span>";
    
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

String WebConfig::escapeJson(const String& input) {
    String output = input;
    output.replace("\\", "\\\\");
    output.replace("\"", "\\\"");
    output.replace("\n", "\\n");
    output.replace("\r", "\\r");
    output.replace("\t", "\\t");
    return output;
}

void WebConfig::sendResponse(int code, const String& contentType, const String& content) {
    if (server) {
        server->send(code, contentType, content);
    }
}
