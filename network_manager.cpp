#include "network_manager.h"
#include "system_monitor.h"
#include "display_manager.h"
#include "ota_manager.h"
#include "web_config.h"
#include "device_health.h"
#include "logging.h"
#include "config_storage.h"
#include <ArduinoOTA.h>
#include <time.h>

// Global instances
WiFiManager wifiManager;
extern WebConfig webConfig;
extern ConfigStorage configStorage;
extern bool firstImageLoaded;  // From main .ino file

// Static member initialization
unsigned long WiFiManager::lastScanTime = 0;

WiFiManager::WiFiManager() :
    wifiConnected(false),
    lastConnectionAttempt(0),
    connectionAttempts(0),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false)
{
}

bool WiFiManager::begin() {
    // Check WiFi credentials
    if (strlen(WIFI_SSID) == 0) {
        LOG_ERROR("ERROR: WiFi SSID is empty - please configure in config.cpp!");
        if (debugPrintFunc) debugPrintFunc("ERROR: WiFi SSID not configured", COLOR_RED);
        return false;
    }
    
    WiFi.mode(WIFI_STA);
    return true;
}

void WiFiManager::connectToWiFi() {
    unsigned long now = millis();
    
    // Don't attempt reconnection too frequently
    if (now - lastConnectionAttempt < WIFI_RETRY_DELAY * 4) {
        return;
    }
    
    lastConnectionAttempt = now;
    LOG_INFO_F("Connecting to WiFi: %s\n", WIFI_SSID);
    if (debugPrintFunc) debugPrintFunc("Connecting to WiFi...", COLOR_YELLOW);
    LOG_DEBUG_F("[WiFi] MAC Address: %s\n", WiFi.macAddress().c_str());
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    connectionAttempts = 0;
    unsigned long startTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < WIFI_MAX_ATTEMPTS) {
        // Check for timeout to prevent infinite hanging
        if (millis() - startTime > WIFI_MAX_WAIT_TIME) {
            LOG_INFO_F("[WiFi] Connection timeout after %lu ms (max: %d ms)\n", millis() - startTime, WIFI_MAX_WAIT_TIME);
            if (debugPrintFunc) debugPrintFunc("WiFi connection timeout!", COLOR_RED);
            break;
        }
        
        systemMonitor.safeDelay(WIFI_RETRY_DELAY);
        systemMonitor.resetWatchdog(); // Explicitly reset watchdog during connection attempts
        connectionAttempts++;
        
        if (connectionAttempts % 4 == 0) {
            systemMonitor.resetWatchdog(); // Additional watchdog reset
        }
        
        // Allow other tasks to run
        yield();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        LOG_INFO_F("✓ WiFi connected - IP: %s\n", WiFi.localIP().toString().c_str());
        if (debugPrintFunc && debugPrintfFunc) {
            debugPrintfFunc(COLOR_GREEN, "%s :: %s", WIFI_SSID, WiFi.localIP().toString().c_str());
            debugPrintfFunc(COLOR_WHITE, " ");  // Group spacing
        }
        LOG_DEBUG_F("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        LOG_DEBUG_F("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
        LOG_DEBUG_F("[WiFi] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
        LOG_DEBUG_F("[WiFi] Connection took %d attempts, %lu ms\n", connectionAttempts, millis() - startTime);
        
        // Sync NTP time after successful connection
        syncNTPTime();
    } else {
        wifiConnected = false;
        LOG_ERROR_F("[WiFi] Connection failed after %d attempts\n", connectionAttempts);
        LOG_INFO_F("[WiFi] WiFi status code: %d\n", WiFi.status());
        LOG_ERROR_F("[WiFi] Status meanings: 0=IDLE, 1=NO_SSID_AVAIL, 3=CONNECTED, 4=CONNECT_FAILED, 6=DISCONNECTED\n");
        
        // Disconnect to clean up any partial connection state
        LOG_INFO("[WiFi] Cleaning up connection state...");
        WiFi.disconnect();
        systemMonitor.safeDelay(1000);
    }
}

bool WiFiManager::isConnected() const {
    return wifiConnected && (WiFi.status() == WL_CONNECTED);
}

void WiFiManager::checkConnection() {
    bool currentStatus = (WiFi.status() == WL_CONNECTED);
    
    if (currentStatus != wifiConnected) {
        wifiConnected = currentStatus;
        if (!currentStatus) {
            LOG_INFO("[WiFi] Connection lost! Starting reconnection logic...");
            LOG_INFO_F("[WiFi] Last known IP: %s\n", WiFi.localIP().toString().c_str());
            LOG_INFO_F("[WiFi] WiFi status: %d\n", WiFi.status());
            // Track disconnect for health monitoring
            DeviceHealthAnalyzer::recordNetworkDisconnect();
        } else {
            LOG_INFO("[WiFi] Connection restored!");
            LOG_INFO_F("[WiFi] IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
    }
}

String WiFiManager::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    }
    return "Not connected";
}

String WiFiManager::getMACAddress() const {
    return WiFi.macAddress();
}

int WiFiManager::getSignalStrength() const {
    if (isConnected()) {
        return WiFi.RSSI();
    }
    return 0;
}

void WiFiManager::setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                                    void (*debugPrintf)(uint16_t, const char*, ...),
                                    bool* firstImageLoadedFlag) {
    debugPrintFunc = debugPrint;
    debugPrintfFunc = debugPrintf;
    if (firstImageLoadedFlag) {
        firstImageLoaded = *firstImageLoadedFlag;
    }
}

void WiFiManager::update() {
    checkConnection();
    
    // Attempt reconnection if disconnected
    if (!isConnected()) {
        connectToWiFi();
    }
}

void WiFiManager::printConnectionInfo() {
    LOG_INFO("=== WiFi Connection Info ===");
    LOG_INFO_F("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    if (isConnected()) {
        LOG_INFO_F("SSID: %s\n", WiFi.SSID().c_str());
        LOG_INFO_F("IP Address: %s\n", getIPAddress().c_str());
        LOG_INFO_F("Signal Strength: %d dBm\n", getSignalStrength());
        LOG_INFO_F("MAC Address: %s\n", getMACAddress().c_str());
        LOG_INFO_F("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        LOG_INFO_F("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    }
    LOG_INFO("============================");
}

bool WiFiManager::startAPMode(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    delay(100);
    
    bool success;
    if (password && strlen(password) > 0) {
        success = WiFi.softAP(ssid, password);
    } else {
        success = WiFi.softAP(ssid);
    }
    
    if (success) {
        LOG_INFO_F("AP Mode started: %s\n", ssid);
        LOG_INFO_F("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    
    return success;
}

void WiFiManager::stopAPMode() {
    WiFi.softAPdisconnect(true);
    LOG_INFO("AP Mode stopped");
}

bool WiFiManager::isAPMode() const {
    return (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA);
}

void WiFiManager::initOTA() {
    // Set OTA hostname
    ArduinoOTA.setHostname("esp32-allsky-display");
    
    // Set OTA port (default is 3232)
    ArduinoOTA.setPort(3232);
    
    // Optional: Set OTA password
    // ArduinoOTA.setPassword("admin");
    
    // OTA callbacks
    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else { // U_SPIFFS
            type = "filesystem";
        }
        
        LOG_INFO_F("Start OTA updating %s\n", type.c_str());
        webConfig.setOTAInProgress(true);  // Suspend main loop operations
        displayManager.showOTAProgress("ArduinoOTA Update", 0, "Starting...");
        
        otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Starting OTA update...");
    });
    
    ArduinoOTA.onEnd([]() {
        LOG_INFO("\nOTA Update Complete");
        displayManager.showOTAProgress("OTA Complete!", 100, "Rebooting...");
        delay(2000);
        
        webConfig.setOTAInProgress(false);  // Resume main loop operations
        otaManager.setStatus(OTA_UPDATE_SUCCESS, "OTA update successful");
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint8_t percent = (progress * 100) / total;
        
        // Only log progress to serial, don't update display
        static uint8_t lastPercent = 0;
        if (percent != lastPercent && percent % 10 == 0) {
            LOG_INFO_F("OTA Progress: %u%%\n", percent);
            otaManager.setProgress(percent);
            lastPercent = percent;
        }
        systemMonitor.forceResetWatchdog();
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        LOG_ERROR_F("OTA Error[%u]: ", error);
        String errorMsg = "";
        
        if (error == OTA_AUTH_ERROR) {
            errorMsg = "Auth Failed";
            LOG_ERROR("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            errorMsg = "Begin Failed";
            LOG_ERROR("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            errorMsg = "Connect Failed";
            LOG_ERROR("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            errorMsg = "Receive Failed";
            LOG_ERROR("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            errorMsg = "End Failed";
            LOG_ERROR("End Failed");
        }
        
        displayManager.showOTAProgress("OTA Error", 0, errorMsg.c_str());
        delay(3000);
        
        webConfig.setOTAInProgress(false);  // Resume main loop operations
        otaManager.setStatus(OTA_UPDATE_FAILED, errorMsg.c_str());
    });
    
    ArduinoOTA.begin();
    LOG_INFO("ArduinoOTA initialized");
    if (debugPrintFunc) debugPrintFunc("ArduinoOTA ready", COLOR_GREEN);
}

void WiFiManager::handleOTA() {
    ArduinoOTA.handle();
}

void WiFiManager::syncNTPTime() {
    if (!isConnected()) {
        LOG_WARNING("[NTP] Cannot sync time - WiFi not connected");
        return;
    }
    
    if (!configStorage.getNTPEnabled()) {
        LOG_INFO("[NTP] Time synchronization disabled in config");
        return;
    }
    
    String ntpServer = configStorage.getNTPServer();
    String timezone = configStorage.getTimezone();
    
    LOG_DEBUG_F("[NTP] Synchronizing time from %s...\n", ntpServer.c_str());
    LOG_DEBUG_F("[NTP] Timezone: %s\n", timezone.c_str());
    
    // Show NTP start message on display
    if (debugPrintFunc && !firstImageLoaded) {
        debugPrintFunc("Updating NTP...", COLOR_YELLOW);
    }
    
    // Configure time with NTP server and timezone
    // GMT offset and daylight offset are handled by the timezone string
    configTime(0, 0, ntpServer.c_str());
    setenv("TZ", timezone.c_str(), 1);
    tzset();
    
    // Wait for time to be set (up to 5 seconds)
    int retries = 0;
    const int maxRetries = 10;
    struct tm timeinfo;
    
    while (!getLocalTime(&timeinfo, 500) && retries < maxRetries) {
        LOG_DEBUG("[NTP] Waiting for time sync...");
        retries++;
        systemMonitor.resetWatchdog();
    }
    
    if (retries >= maxRetries) {
        LOG_ERROR("[NTP] Failed to synchronize time");
        if (debugPrintFunc) debugPrintFunc("NTP sync failed", COLOR_RED);
    } else {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
        LOG_INFO_F("✓ Time synced: %s\n", timeStr);
        if (debugPrintfFunc && !firstImageLoaded) {
            debugPrintfFunc(COLOR_CYAN, "%s", timeStr);
            debugPrintfFunc(COLOR_WHITE, " ");  // Group spacing
        }
    }
}

bool WiFiManager::isTimeValid() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) {
        return false;
    }
    // Check if year is reasonable (>2020)
    return (timeinfo.tm_year + 1900) > 2020;
}

int WiFiManager::scanNetworks(bool async, bool show_hidden) {
    LOG_INFO_F("[WiFi] Starting network scan (async=%d, show_hidden=%d)\n", async, show_hidden);
    
    // Rate limiting - enforce minimum 10 second gap between scans
    unsigned long now = millis();
    if (now - lastScanTime < 10000) {
        LOG_WARNING_F("[WiFi] Scan rate limited - last scan was %lu ms ago (min: 10000 ms)\n", now - lastScanTime);
        return -1;  // Rate limited
    }
    
    lastScanTime = now;
    
    // Start scan - async=false for blocking, show_hidden=true to include hidden networks
    int n = WiFi.scanNetworks(async, show_hidden);
    
    if (n == WIFI_SCAN_RUNNING) {
        LOG_DEBUG("[WiFi] Async scan started");
        return WIFI_SCAN_RUNNING;
    } else if (n == WIFI_SCAN_FAILED) {
        LOG_ERROR("[WiFi] Network scan failed");
        return WIFI_SCAN_FAILED;
    } else {
        LOG_INFO_F("[WiFi] Network scan complete - found %d networks\n", n);
        return n;
    }
}

bool WiFiManager::isScanComplete() {
    int16_t status = WiFi.scanComplete();
    return (status >= 0);
}

String WiFiManager::getScanResultsJSON() {
    int n = WiFi.scanComplete();
    
    if (n == WIFI_SCAN_RUNNING) {
        return "{\"status\":\"scanning\",\"message\":\"Scan in progress\"}";
    } else if (n == WIFI_SCAN_FAILED) {
        return "{\"status\":\"error\",\"message\":\"Scan failed\"}";
    } else if (n == 0) {
        return "{\"status\":\"success\",\"networks\":[]}";
    }
    
    // Build JSON response with only 2.4GHz networks
    String json = "{\"status\":\"success\",\"networks\":[";
    
    int count = 0;
    for (int i = 0; i < n; i++) {
        uint8_t channel = WiFi.channel(i);
        
        // Only include 2.4GHz networks (channels 1-13)
        if (channel >= 1 && channel <= 13) {
            if (count > 0) json += ",";
            
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            wifi_auth_mode_t encryption = WiFi.encryptionType(i);
            
            // Handle hidden networks
            String displaySSID = (ssid.length() == 0) ? "[Hidden Network]" : ssid;
            
            // Determine encryption type string
            String encType;
            switch (encryption) {
                case WIFI_AUTH_OPEN: encType = "Open"; break;
                case WIFI_AUTH_WEP: encType = "WEP"; break;
                case WIFI_AUTH_WPA_PSK: encType = "WPA"; break;
                case WIFI_AUTH_WPA2_PSK: encType = "WPA2"; break;
                case WIFI_AUTH_WPA_WPA2_PSK: encType = "WPA/WPA2"; break;
                case WIFI_AUTH_WPA2_ENTERPRISE: encType = "WPA2-EAP"; break;
                case WIFI_AUTH_WPA3_PSK: encType = "WPA3"; break;
                case WIFI_AUTH_WPA2_WPA3_PSK: encType = "WPA2/WPA3"; break;
                default: encType = "Unknown"; break;
            }
            
            // Calculate signal quality
            String quality;
            if (rssi > -50) quality = "Excellent";
            else if (rssi > -60) quality = "Good";
            else if (rssi > -70) quality = "Fair";
            else if (rssi > -80) quality = "Poor";
            else quality = "Weak";
            
            // Escape SSID for JSON
            displaySSID.replace("\\", "\\\\");
            displaySSID.replace("\"", "\\\"");
            
            json += "{";
            json += "\"ssid\":\"" + displaySSID + "\",";
            json += "\"rssi\":" + String(rssi) + ",";
            json += "\"channel\":" + String(channel) + ",";
            json += "\"encryption\":\"" + encType + "\",";
            json += "\"quality\":\"" + quality + "\",";
            json += "\"is_open\":" + String(encryption == WIFI_AUTH_OPEN ? "true" : "false");
            json += "}";
            
            count++;
        }
    }
    
    json += "],\"count\":" + String(count) + "}";
    
    LOG_INFO_F("[WiFi] Returning %d 2.4GHz networks (filtered from %d total)\n", count, n);
    
    // Clear scan results to free memory
    WiFi.scanDelete();
    
    return json;
}
