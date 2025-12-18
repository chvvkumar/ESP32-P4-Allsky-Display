#include "web_config.h"
#include "web_config_html.h"
#include "build_info.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "crash_logger.h"
#include "logging.h"
#include <time.h>

// Global instance
WebConfig webConfig;

WebConfig::WebConfig() : server(nullptr), wsServer(nullptr), serverRunning(false), otaInProgress(false) {}

bool WebConfig::begin(int port) {
    if (serverRunning) {
        LOG_DEBUG_F("[WebServer] Already running on port %d\n", port);
        return true;
    }
    
    try {
        LOG_DEBUG_F("[WebServer] Initializing web server on port %d\n", port);
        server = new WebServer(port);
        
        if (!server) {
            LOG_CRITICAL("[WebServer] Failed to allocate WebServer memory!");
            return false;
        }
        
        LOG_DEBUG("[WebServer] Setting up HTTP routes");
        
        // Setup routes
        server->on("/", [this]() { handleRoot(); });
        server->on("/console", [this]() { handleConsole(); });
        server->on("/config/network", [this]() { handleNetworkConfig(); });
        server->on("/config/mqtt", [this]() { handleMQTTConfig(); });
        server->on("/config/images", [this]() { handleImageConfig(); });
        server->on("/config/display", [this]() { handleDisplayConfig(); });
        server->on("/config/system", [this]() { handleAdvancedConfig(); });
        server->on("/config/commands", [this]() { handleSerialCommands(); });
        server->on("/status", [this]() { handleStatus(); });
        server->on("/api/save", HTTP_POST, [this]() { handleSaveConfig(); });
        server->on("/api/add-source", HTTP_POST, [this]() { handleAddImageSource(); });
        server->on("/api/remove-source", HTTP_POST, [this]() { handleRemoveImageSource(); });
        server->on("/api/update-source", HTTP_POST, [this]() { handleUpdateImageSource(); });
    server->on("/api/clear-sources", HTTP_POST, [this]() { handleClearImageSources(); });
    server->on("/api/bulk-delete-sources", HTTP_POST, [this]() { handleBulkDeleteImageSources(); });
    server->on("/api/next-image", HTTP_POST, [this]() { handleNextImage(); });
    server->on("/api/force-refresh", HTTP_POST, [this]() { handleForceRefresh(); });
        server->on("/api/update-transform", HTTP_POST, [this]() { handleUpdateImageTransform(); });
        server->on("/api/copy-defaults", HTTP_POST, [this]() { handleCopyDefaultsToImage(); });
        server->on("/api/apply-transform", HTTP_POST, [this]() { handleApplyTransform(); });
        server->on("/api/toggle-image-enabled", HTTP_POST, [this]() { handleToggleImageEnabled(); });
        server->on("/api/select-image", HTTP_POST, [this]() { handleSelectImage(); });
        server->on("/api/clear-editing-state", HTTP_POST, [this]() { handleClearEditingState(); });
        server->on("/api/update-image-duration", HTTP_POST, [this]() { handleUpdateImageDuration(); });
        server->on("/api/restart", HTTP_POST, [this]() { handleRestart(); });
        server->on("/api/factory-reset", HTTP_POST, [this]() { handleFactoryReset(); });
        server->on("/api/set-log-severity", HTTP_POST, [this]() { handleSetLogSeverity(); });
        server->on("/api/clear-crash-logs", HTTP_POST, [this]() { handleClearCrashLogs(); });
        server->on("/api/force-brightness-update", HTTP_POST, [this]() { handleForceBrightnessUpdate(); });
        server->on("/api/info", HTTP_GET, [this]() { handleGetAllInfo(); });
        server->on("/api/current-image", HTTP_GET, [this]() { handleCurrentImage(); });
        server->on("/api/health", HTTP_GET, [this]() { handleGetHealth(); });
        
        // Favicon handler (prevents 404 log clutter when browsers request favicon)
        server->on("/favicon.ico", HTTP_GET, [this]() { 
            server->send(204); // No Content - silently ignore favicon requests
        });
        
        // Initialize ElegantOTA
        ElegantOTA.begin(server);
        ElegantOTA.onStart([]() {
            LOG_INFO("ElegantOTA: Update started");
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
                LOG_DEBUG_F("ElegantOTA Progress: %u%%\n", percent);
                lastPercent = percent;
            }
        });
        ElegantOTA.onEnd([](bool success) {
            systemMonitor.forceResetWatchdog();
            webConfig.setOTAInProgress(false);  // Re-enable WebSocket
            if (success) {
                LOG_INFO("ElegantOTA: Update successful!");
                displayManager.showOTAProgress("OTA Complete!", 100, "Rebooting...");
                delay(2000);
            } else {
                LOG_ERROR("ElegantOTA: Update failed!");
                displayManager.showOTAProgress("OTA Failed", 0, "Update failed");
                delay(3000);
            }
        });
        server->on("/api-reference", [this]() { handleAPIReference(); });
        server->onNotFound([this]() { handleNotFound(); });
        
        LOG_DEBUG("Starting WebServer...");
        server->begin();
        
        // Initialize WebSocket server on port 81
        LOG_DEBUG("[WebSocket] Starting WebSocket server on port 81");
        LOG_DEBUG_F("[WebSocket] Free heap before allocation: %d bytes\n", ESP.getFreeHeap());
        wsServer = new WebSocketsServer(81);
        if (wsServer) {
            LOG_DEBUG("[WebSocket] Server instance created successfully");
            wsServer->begin();
            wsServer->onEvent(webSocketEvent);
            LOG_DEBUG("[WebSocket] ✓ Server started and event handler registered");
            LOG_DEBUG_F("[WebSocket] Listening on port 81 (clients can connect to ws://%s:81)\n", WiFi.localIP().toString().c_str());
        } else {
            LOG_ERROR("[WebSocket] ERROR: Failed to allocate WebSocket server!");
            LOG_ERROR_F("[WebSocket] Free heap: %d bytes, PSRAM: %d bytes\n", ESP.getFreeHeap(), ESP.getFreePsram());
        }
        
        if (!server) {
            LOG_ERROR("ERROR: WebServer failed to start!");
            return false;
        }
        
        serverRunning = true;
        LOG_DEBUG_F("✓ Web configuration server started successfully on port %d\n", port);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_F("ERROR: Exception starting web server: %s\n", e.what());
        return false;
    } catch (...) {
        LOG_ERROR("ERROR: Unknown exception starting web server!");
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
    size_t heapBefore = ESP.getFreeHeap();
    LOG_DEBUG_F("[WebServer] Dashboard page accessed (heap before: %d bytes)\n", heapBefore);
    
    String html = generateHeader("Dashboard");
    html += generateNavigation("dashboard");
    html += generateMainPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
    
    size_t heapAfter = ESP.getFreeHeap();
    int heapDelta = (int)heapBefore - (int)heapAfter;
    if (heapDelta > 0) {
        LOG_WARNING_F("[WebServer] Dashboard request used %d bytes heap (before: %d, after: %d)\n", 
                      heapDelta, heapBefore, heapAfter);
    } else {
        LOG_DEBUG_F("[WebServer] Dashboard completed (heap after: %d bytes)\n", heapAfter);
    }
}

void WebConfig::handleConsole() {
    LOG_DEBUG("[WebServer] Serial console page accessed");
    String html = generateHeader("Serial Console");
    html += generateNavigation("console");
    html += generateConsolePage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleNetworkConfig() {
    LOG_DEBUG("[WebServer] Network configuration page accessed");
    String html = generateHeader("Network Configuration");
    html += generateNavigation("network");
    html += generateNetworkPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleMQTTConfig() {
    LOG_DEBUG("[WebServer] MQTT configuration page accessed");
    String html = generateHeader("MQTT Configuration");
    html += generateNavigation("mqtt");
    html += generateMQTTPage();
    html += generateFooter();
    sendResponse(200, "text/html", html);
}

void WebConfig::handleImageConfig() {
    LOG_DEBUG("[WebServer] Image sources page accessed");
    String html = generateHeader("Image Sources");
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
    String html = generateHeader("System Configuration");
    html += generateNavigation("system");
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
    String uri = server->uri();
    LOG_WARNING_F("[WebServer] 404 Not Found: %s\n", uri.c_str());
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
    String html;
    html.reserve(2000);  // Pre-allocate ~2KB for header HTML
    html = "<!DOCTYPE html><html lang='en'><head>";
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
    String html;
    html.reserve(1500);  // Pre-allocate ~1.5KB for navigation HTML
    html = "<div class='nav'><div class='container' style='position:relative'>";
    html += "<button class='nav-toggle' onclick='toggleNav()' aria-label='Toggle navigation'><i class='fas fa-bars'></i></button>";
    html += "<div class='nav-content'>";
    
    String pages[] = {"dashboard", "images", "display", "network", "mqtt", "console", "system", "commands", "api"};
    String labels[] = {"🏠 Dashboard", "🖼️ Images", "💡 Display", "📡 Network", "🔗 MQTT", "🖥️ Console", "⚙️ System", "📟 Commands", "📚 API"};
    String urls[] = {"/", "/config/images", "/config/display", "/config/network", "/config/mqtt", "/console", "/config/system", "/config/commands", "/api-reference"};
    
    for (int i = 0; i < 9; i++) {
        String activeClass = (currentPage == pages[i]) ? " active" : "";
        html += "<a href='" + urls[i] + "' class='nav-item" + activeClass + "'>" + labels[i] + "</a>";
    }
    
    html += "</div></div></div>";
    return html;
}

String WebConfig::generateFooter() {
    String html;
    html.reserve(1000);  // Pre-allocate ~1KB for footer HTML
    html = "<script>" + String(FPSTR(HTML_JAVASCRIPT)) + "</script>";
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
    String json;
    json.reserve(512);  // Pre-allocate ~512 bytes for system status JSON
    json = "{";
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
    String html;
    html.reserve(256);  // Pre-allocate ~256 bytes for status badges
    html = "";
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
            LOG_DEBUG_F("[WebSocket] Client #%u disconnected\n", num);
            LOG_DEBUG_F("[WebSocket] Active clients: %d\n", webConfig.wsServer->connectedClients());
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webConfig.wsServer->remoteIP(num);
                LOG_INFO_F("[WebSocket] Client #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                LOG_DEBUG_F("[WebSocket] Total active clients: %d\n", webConfig.wsServer->connectedClients());
                // Send welcome message
                String welcome = "[SYSTEM] Console connected. Monitoring serial output...\n";
                webConfig.wsServer->sendTXT(num, welcome);
                LOG_DEBUG_F("[WebSocket] Welcome message sent to client #%u\n", num);
                // Send buffered crash logs to new client
                webConfig.sendCrashLogsToClient(num);
            }
            break;
        case WStype_TEXT:
            LOG_DEBUG_F("[WebSocket] Received from client #%u: %s\n", num, payload);
            break;
        case WStype_ERROR:
            LOG_ERROR_F("[WebSocket] ERROR on client #%u\n", num);
            break;
        case WStype_PING:
            LOG_DEBUG_F("[WebSocket] Ping from client #%u\n", num);
            break;
        case WStype_PONG:
            LOG_DEBUG_F("[WebSocket] Pong from client #%u\n", num);
            break;
        default:
            LOG_WARNING_F("[WebSocket] Unknown event type %d from client #%u\n", type, num);
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
    
    // Get current time - always use real time
    struct tm timeinfo;
    char timeStr[32];
    if (getLocalTime(&timeinfo, 0)) {
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        // Show clear message that time is not synced yet
        snprintf(timeStr, sizeof(timeStr), "TIME_NOT_SYNCED");
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
