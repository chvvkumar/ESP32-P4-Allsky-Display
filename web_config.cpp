#include "web_config.h"
#include "web_config_html.h"
#include "build_info.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "crash_logger.h"
#include <time.h>

// Global instance
WebConfig webConfig;

WebConfig::WebConfig() : server(nullptr), wsServer(nullptr), serverRunning(false), otaInProgress(false) {}

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
        server->on("/console", [this]() { handleConsole(); });
        server->on("/config/network", [this]() { handleNetworkConfig(); });
        server->on("/config/mqtt", [this]() { handleMQTTConfig(); });
        server->on("/config/images", [this]() { handleImageConfig(); });
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
        server->on("/api/set-log-severity", HTTP_POST, [this]() { handleSetLogSeverity(); });
        server->on("/api/clear-crash-logs", HTTP_POST, [this]() { handleClearCrashLogs(); });
        server->on("/api/info", HTTP_GET, [this]() { handleGetAllInfo(); });
        server->on("/api/current-image", HTTP_GET, [this]() { handleCurrentImage(); });
        
        // Initialize ElegantOTA
        ElegantOTA.begin(server);
        ElegantOTA.onStart([]() {
            Serial.println("ElegantOTA: Update started");
            webConfig.setOTAInProgress(true);  // Suppress WebSocket during OTA
            displayManager.showOTAProgress("OTA Update", 0, "Starting...");
            systemMonitor.forceResetWatchdog();
        });
        ElegantOTA.onProgress([](size_t current, size_t final) {
            // Reset watchdog on every progress update to prevent timeout
            systemMonitor.forceResetWatchdog();
            
            // Only log progress to serial, don't update display
            static uint8_t lastPercent = 0;
            uint8_t percent = (current * 100) / final;
            if (percent != lastPercent && percent % 10 == 0) {
                Serial.printf("ElegantOTA Progress: %u%%\n", percent);
                lastPercent = percent;
            }
        });
        ElegantOTA.onEnd([](bool success) {
            systemMonitor.forceResetWatchdog();
            webConfig.setOTAInProgress(false);  // Re-enable WebSocket
            if (success) {
                Serial.println("ElegantOTA: Update successful!");
                displayManager.showOTAProgress("OTA Complete!", 100, "Rebooting...");
                delay(2000);
            } else {
                Serial.println("ElegantOTA: Update failed!");
                displayManager.showOTAProgress("OTA Failed", 0, "Update failed");
                delay(3000);
            }
        });
        server->on("/api-reference", [this]() { handleAPIReference(); });
        server->onNotFound([this]() { handleNotFound(); });
        
        Serial.println("Starting WebServer...");
        server->begin();
        
        // Initialize WebSocket server on port 81
        Serial.println("[WebSocket] Starting WebSocket server on port 81...");
        Serial.printf("[WebSocket] Free heap before allocation: %d bytes\n", ESP.getFreeHeap());
        wsServer = new WebSocketsServer(81);
        if (wsServer) {
            Serial.println("[WebSocket] Server instance created successfully");
            wsServer->begin();
            wsServer->onEvent(webSocketEvent);
            Serial.println("[WebSocket] ✓ Server started and event handler registered");
            Serial.printf("[WebSocket] Listening on port 81 (clients can connect to ws://%s:81)\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("[WebSocket] ERROR: Failed to allocate WebSocket server!");
            Serial.printf("[WebSocket] Free heap: %d bytes, PSRAM: %d bytes\n", ESP.getFreeHeap(), ESP.getFreePsram());
        }
        
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
    if (wsServer) {
        wsServer->loop();
    }
}

void WebConfig::loopWebSocket() {
    if (wsServer) {
        wsServer->loop();
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
    if (wsServer) {
        wsServer->close();
        delete wsServer;
        wsServer = nullptr;
    }
}

// Route handlers - these call the page generators from web_config_pages.cpp
void WebConfig::handleRoot() {
    String html = generateHeader("Dashboard");
    html += generateNavigation("dashboard");
    html += generateMainPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleConsole() {
    String html = generateHeader("Serial Console");
    html += generateNavigation("console");
    html += generateConsolePage();
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
    html += generateNavigation("images");
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
    
    String pages[] = {"dashboard", "images", "display", "network", "mqtt", "console", "commands", "advanced", "api"};
    String labels[] = {"🏠 Dashboard", "🖼️ Images", "💡 Display", "📡 Network", "🔗 MQTT", "🖥️ Console", "📟 Commands", "⚙️ Advanced", "📚 API"};
    String urls[] = {"/", "/config/images", "/config/display", "/config/network", "/config/mqtt", "/console", "/config/commands", "/config/advanced", "/api-reference"};
    
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

// WebSocket event handler (static)
void WebConfig::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WebSocket] Client #%u disconnected\n", num);
            Serial.printf("[WebSocket] Active clients: %d\n", webConfig.wsServer->connectedClients());
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webConfig.wsServer->remoteIP(num);
                Serial.printf("[WebSocket] Client #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("[WebSocket] Total active clients: %d\n", webConfig.wsServer->connectedClients());
                // Send welcome message
                String welcome = "[SYSTEM] Console connected. Monitoring serial output...\n";
                webConfig.wsServer->sendTXT(num, welcome);
                Serial.printf("[WebSocket] Welcome message sent to client #%u\n", num);
                // Send buffered crash logs to new client
                webConfig.sendCrashLogsToClient(num);
            }
            break;
        case WStype_TEXT:
            Serial.printf("[WebSocket] Received from client #%u: %s\n", num, payload);
            break;
        case WStype_ERROR:
            Serial.printf("[WebSocket] ERROR on client #%u\n", num);
            break;
        case WStype_PING:
            Serial.printf("[WebSocket] Ping from client #%u\n", num);
            break;
        case WStype_PONG:
            Serial.printf("[WebSocket] Pong from client #%u\n", num);
            break;
        default:
            Serial.printf("[WebSocket] Unknown event type %d from client #%u\n", type, num);
            break;
    }
}

// Broadcast log message to all connected WebSocket clients with severity filtering
void WebConfig::broadcastLog(const char* message, uint16_t color, LogSeverity severity) {
    if (!serverRunning || !wsServer || !message || otaInProgress) {
        // Silent return - don't spam serial with these conditions
        return;
    }
    
    // Check severity filter - only send messages at or above configured threshold
    int minSeverity = configStorage.getMinLogSeverity();
    if (severity < minSeverity) {
        return; // Message filtered out by severity level
    }
    
    // Log broadcast failures for troubleshooting
    static unsigned long lastBroadcastError = 0;
    if (wsServer->connectedClients() == 0) {
        // Only log "no clients" once per 30 seconds to avoid spam
        if (millis() - lastBroadcastError > 30000) {
            Serial.println("[WebSocket] DEBUG: No clients connected to broadcast to");
            lastBroadcastError = millis();
        }
        return;
    }
    
    // Severity prefixes for visual identification
    const char* severityPrefix = "";
    switch(severity) {
        case LOG_DEBUG: severityPrefix = "[DEBUG] "; break;
        case LOG_INFO: severityPrefix = "[INFO] "; break;
        case LOG_WARNING: severityPrefix = "[WARN] "; break;
        case LOG_ERROR: severityPrefix = "[ERROR] "; break;
        case LOG_CRITICAL: severityPrefix = "[CRITICAL] "; break;
    }
    
    // Use fixed buffer to avoid String heap fragmentation
    char buffer[384];
    
    // Get current time
    struct tm timeinfo;
    char timeStr[32];
    if (getLocalTime(&timeinfo, 0)) {
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        // Fallback to relative time if NTP not synced
        unsigned long ms = millis();
        snprintf(timeStr, sizeof(timeStr), "%lu.%03lu", ms / 1000, ms % 1000);
    }
    
    int written = snprintf(buffer, sizeof(buffer), "[%s] %s%s", 
                          timeStr, severityPrefix, message);
    
    // Ensure newline termination if there's room
    if (written > 0 && written < (int)sizeof(buffer) - 2) {
        if (buffer[written - 1] != '\n') {
            buffer[written] = '\n';
            buffer[written + 1] = '\0';
        }
    }
    
    wsServer->broadcastTXT(buffer);
}

// Send crash logs to a specific WebSocket client
void WebConfig::sendCrashLogsToClient(uint8_t clientNum) {
    // Get recent logs from crash logger
    String logs = crashLogger.getRecentLogs(6144); // Get up to 6KB of logs
    
    if (logs.length() > 0) {
        Serial.printf("[WebSocket] Sending %d bytes of crash logs to client #%u\n", logs.length(), clientNum);
        
        // Send header to distinguish historical logs from live stream
        String header = "\n╔══════════════════════════════════════════════════════════════╗\n";
        header += "║           BUFFERED LOGS (Boot + Crash History)              ║\n";
        header += "║  These are preserved messages from boot and previous crashes ║\n";
        header += "╚══════════════════════════════════════════════════════════════╝\n\n";
        wsServer->sendTXT(clientNum, header);
        delay(20);
        
        // Send in chunks to avoid WebSocket buffer overflow
        const size_t chunkSize = 1024;
        size_t offset = 0;
        
        while (offset < logs.length()) {
            size_t remaining = logs.length() - offset;
            size_t sendSize = (remaining < chunkSize) ? remaining : chunkSize;
            
            String chunk = logs.substring(offset, offset + sendSize);
            wsServer->sendTXT(clientNum, chunk);
            
            offset += sendSize;
            delay(10); // Small delay to prevent buffer overflow
        }
        
        // Send footer to mark end of historical logs
        String footer = "\n╔══════════════════════════════════════════════════════════════╗\n";
        footer += "║               END OF BUFFERED LOGS                           ║\n";
        footer += "║        Live log streaming continues below...                 ║\n";
        footer += "╚══════════════════════════════════════════════════════════════╝\n\n";
        wsServer->sendTXT(clientNum, footer);
        
        Serial.printf("[WebSocket] Crash logs sent to client #%u - now streaming live\n", clientNum);
    } else {
        Serial.printf("[WebSocket] No crash logs - client #%u will receive live stream only\n", clientNum);
        String noLogs = "[SYSTEM] No buffered logs. Streaming live output...\n\n";
        wsServer->sendTXT(clientNum, noLogs);
    }
}
