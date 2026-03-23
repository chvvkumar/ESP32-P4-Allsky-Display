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
    connectionInProgress(false),
    connectionStartTime(0),
    reconnectBackoff(RECONNECT_BACKOFF_MIN),
    ntpSyncInProgress(false),
    ntpSyncStartTime(0),
    ntpRetries(0),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false),
    roamScanPending(false),
    roamInProgress(false),
    lastRoamScanTime(0),
    roamStartTime(0),
    roamTargetChannel(0)
{
    memset(roamTargetBSSID, 0, sizeof(roamTargetBSSID));
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

    // If a connection attempt is already in progress, poll its status
    if (connectionInProgress) {
        // Check for timeout
        if (now - connectionStartTime > WIFI_MAX_WAIT_TIME) {
            LOG_INFO_F("[WiFi] Connection timeout after %lu ms (max: %d ms)\n",
                       now - connectionStartTime, WIFI_MAX_WAIT_TIME);
            if (debugPrintFunc) debugPrintFunc("WiFi connection timeout!", COLOR_RED);
            connectionInProgress = false;
            wifiConnected = false;

            // Exponential backoff for next attempt
            reconnectBackoff = min(reconnectBackoff * 2, RECONNECT_BACKOFF_MAX);
            LOG_INFO_F("[WiFi] Next reconnect backoff: %lu ms\n", reconnectBackoff);

            WiFi.disconnect();
            return;
        }

        // Poll connection status (non-blocking)
        if (WiFi.status() == WL_CONNECTED) {
            connectionInProgress = false;
            wifiConnected = true;
            reconnectBackoff = RECONNECT_BACKOFF_MIN;  // Reset backoff on success

            LOG_INFO_F("WiFi connected - IP: %s\n", WiFi.localIP().toString().c_str());
            if (debugPrintFunc && debugPrintfFunc) {
                debugPrintfFunc(COLOR_GREEN, "%s :: %s", WIFI_SSID, WiFi.localIP().toString().c_str());
                debugPrintfFunc(COLOR_WHITE, " ");  // Group spacing
            }
            LOG_DEBUG_F("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            LOG_DEBUG_F("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
            LOG_DEBUG_F("[WiFi] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
            LOG_DEBUG_F("[WiFi] Connection took %lu ms\n", now - connectionStartTime);

            // Start NTP time sync (non-blocking)
            syncNTPTime();
        } else if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
            // Definite failure — stop waiting
            connectionInProgress = false;
            wifiConnected = false;
            LOG_ERROR_F("[WiFi] Connection failed, status: %d\n", WiFi.status());

            reconnectBackoff = min(reconnectBackoff * 2, RECONNECT_BACKOFF_MAX);
            WiFi.disconnect();
        }
        // Otherwise still connecting — just return and check next loop iteration
        return;
    }

    // Rate-limit new connection attempts using exponential backoff
    if (now - lastConnectionAttempt < reconnectBackoff) {
        return;
    }

    // Start a new non-blocking connection attempt
    lastConnectionAttempt = now;
    connectionStartTime = now;
    connectionInProgress = true;

    LOG_INFO_F("Connecting to WiFi: %s\n", WIFI_SSID);
    if (debugPrintFunc) debugPrintFunc("Connecting to WiFi...", COLOR_YELLOW);
    LOG_DEBUG_F("[WiFi] MAC Address: %s\n", WiFi.macAddress().c_str());

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // Return immediately — status will be polled on subsequent update() calls
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

void WiFiManager::checkForBetterAP() {
    unsigned long now = millis();

    // --- State: Roam in progress (polling reconnection to target AP) ---
    if (roamInProgress) {
        if (WiFi.status() == WL_CONNECTED) {
            // Roam succeeded
            roamInProgress = false;
            wifiConnected = true;
            LOG_INFO_F("[WiFi] Roam successful - connected to %s (RSSI: %d dBm, ch: %d) in %lu ms\n",
                       WiFi.BSSIDstr().c_str(), WiFi.RSSI(), WiFi.channel(),
                       now - roamStartTime);
            DeviceHealthAnalyzer::recordRoam();
            syncNTPTime();
        } else if (now - roamStartTime > WIFI_MAX_WAIT_TIME) {
            // Roam timed out — fall back to normal reconnection
            LOG_WARNING_F("[WiFi] Roam failed (timeout after %lu ms) - falling back to auto-connect\n",
                          now - roamStartTime);
            roamInProgress = false;
            wifiConnected = false;
            WiFi.disconnect();
            // Next update() cycle will call connectToWiFi() without BSSID lock
        } else if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_NO_SSID_AVAIL) {
            LOG_WARNING_F("[WiFi] Roam failed (status: %d) - falling back to auto-connect\n",
                          WiFi.status());
            roamInProgress = false;
            wifiConnected = false;
            WiFi.disconnect();
        }
        return;
    }

    // Don't start roam logic if not connected or if NTP sync is in progress
    if (!isConnected() || ntpSyncInProgress) {
        return;
    }

    // --- State: Scan pending (poll for async scan completion) ---
    if (roamScanPending) {
        int scanResult = WiFi.scanComplete();

        if (scanResult == WIFI_SCAN_RUNNING) {
            return;  // Still scanning
        }

        roamScanPending = false;

        if (scanResult == WIFI_SCAN_FAILED || scanResult < 0) {
            LOG_WARNING("[WiFi] Roam scan failed");
            WiFi.scanDelete();
            return;
        }

        // Get current connection info for comparison
        int currentRSSI = WiFi.RSSI();
        String currentBSSID = WiFi.BSSIDstr();
        String currentSSID = String(WIFI_SSID);

        // Find the strongest AP with the same SSID but different BSSID
        int bestIndex = -1;
        int bestRSSI = currentRSSI;

        for (int i = 0; i < scanResult; i++) {
            String scanSSID = WiFi.SSID(i);
            String scanBSSID = WiFi.BSSIDstr(i);
            int scanRSSI = WiFi.RSSI(i);

            if (scanSSID == currentSSID && scanBSSID != currentBSSID) {
                if (scanRSSI > bestRSSI + WIFI_ROAM_RSSI_THRESHOLD) {
                    bestRSSI = scanRSSI;
                    bestIndex = i;
                }
            }
        }

        if (bestIndex >= 0) {
            // Found a significantly stronger AP — initiate roam
            LOG_INFO_F("[WiFi] Roaming from %s (%d dBm) to %s (%d dBm, ch: %d)\n",
                       currentBSSID.c_str(), currentRSSI,
                       WiFi.BSSIDstr(bestIndex).c_str(), bestRSSI, WiFi.channel(bestIndex));

            // Save target BSSID and channel before scanDelete clears them
            memcpy(roamTargetBSSID, WiFi.BSSID(bestIndex), 6);
            roamTargetChannel = WiFi.channel(bestIndex);

            WiFi.scanDelete();

            // Disconnect and reconnect to the stronger AP
            WiFi.disconnect();
            roamInProgress = true;
            roamStartTime = now;
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD, roamTargetChannel, roamTargetBSSID);
        } else {
            LOG_DEBUG_F("[WiFi] Roam scan: no better AP found (current: %s, %d dBm, %d candidates)\n",
                        currentBSSID.c_str(), currentRSSI, scanResult);
            WiFi.scanDelete();
        }

        return;
    }

    // --- State: Idle (check if it's time to start a new roam scan) ---
    if (now - lastRoamScanTime >= WIFI_ROAM_CHECK_INTERVAL) {
        // Don't start a scan if one is already running (e.g., web API scan)
        if (WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
            return;
        }

        lastRoamScanTime = now;
        roamScanPending = true;
        DeviceHealthAnalyzer::recordRoamScan();
        WiFi.scanNetworks(true, false);  // async=true, show_hidden=false
        LOG_DEBUG("[WiFi] Roam scan started (async)");
    }
}

void WiFiManager::update() {
    // During a roam, skip checkConnection/connectToWiFi to prevent interference
    if (roamInProgress) {
        checkForBetterAP();  // Poll roam connection status
        return;
    }

    checkConnection();

    if (isConnected()) {
        checkForBetterAP();  // Scan for better APs / evaluate results
    } else {
        // Attempt reconnection if disconnected (non-blocking)
        connectToWiFi();
    }

    // Poll non-blocking NTP sync
    if (ntpSyncInProgress) {
        unsigned long now = millis();
        if (now - ntpSyncStartTime >= ntpRetries * NTP_POLL_INTERVAL) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                // NTP sync succeeded
                ntpSyncInProgress = false;
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
                LOG_INFO_F("Time synced: %s\n", timeStr);
                if (debugPrintfFunc && !firstImageLoaded) {
                    debugPrintfFunc(COLOR_CYAN, "%s", timeStr);
                    debugPrintfFunc(COLOR_WHITE, " ");  // Group spacing
                }
            } else {
                ntpRetries++;
                if (ntpRetries >= NTP_MAX_RETRIES) {
                    ntpSyncInProgress = false;
                    LOG_ERROR("[NTP] Failed to synchronize time");
                    if (debugPrintFunc) debugPrintFunc("NTP sync failed", COLOR_RED);
                }
            }
        }
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

    // If already syncing, don't restart
    if (ntpSyncInProgress) {
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
    configTime(0, 0, ntpServer.c_str());
    setenv("TZ", timezone.c_str(), 1);
    tzset();

    // Start non-blocking NTP poll
    ntpSyncInProgress = true;
    ntpSyncStartTime = millis();
    ntpRetries = 0;
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
    // Pre-reserve ~128 bytes per network to reduce heap fragmentation
    String json;
    json.reserve(128 * n + 64);
    json = "{\"status\":\"success\",\"networks\":[";

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
