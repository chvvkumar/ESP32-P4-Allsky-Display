#include "network_manager.h"
#include "system_monitor.h"
#include "display_manager.h"
#include "ota_manager.h"
#include "web_config.h"
#include <ArduinoOTA.h>

// Global instances
WiFiManager wifiManager;
extern WebConfig webConfig;

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
        Serial.println("ERROR: WiFi SSID is empty - please configure in config.cpp!");
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
    Serial.printf("[WiFi] Attempting connection to SSID: %s\n", WIFI_SSID);
    Serial.printf("[WiFi] MAC Address: %s\n", WiFi.macAddress().c_str());
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    connectionAttempts = 0;
    unsigned long startTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < WIFI_MAX_ATTEMPTS) {
        // Check for timeout to prevent infinite hanging
        if (millis() - startTime > WIFI_MAX_WAIT_TIME) {
            Serial.printf("[WiFi] Connection timeout after %lu ms (max: %d ms)\n", millis() - startTime, WIFI_MAX_WAIT_TIME);
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
        Serial.println("[WiFi] ✓ Connection successful!");
        Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("[WiFi] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
        Serial.printf("[WiFi] Connection took %d attempts, %lu ms\n", connectionAttempts, millis() - startTime);
    } else {
        wifiConnected = false;
        Serial.printf("[WiFi] Connection failed after %d attempts\n", connectionAttempts);
        Serial.printf("[WiFi] WiFi status code: %d\n", WiFi.status());
        Serial.printf("[WiFi] Status meanings: 0=IDLE, 1=NO_SSID_AVAIL, 3=CONNECTED, 4=CONNECT_FAILED, 6=DISCONNECTED\n");
        
        // Disconnect to clean up any partial connection state
        Serial.println("[WiFi] Cleaning up connection state...");
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
            Serial.println("[WiFi] Connection lost! Starting reconnection logic...");
            Serial.printf("[WiFi] Last known IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WiFi] WiFi status: %d\n", WiFi.status());
        } else {
            Serial.println("[WiFi] Connection restored!");
            Serial.printf("[WiFi] IP: %s, RSSI: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
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
    Serial.println("=== WiFi Connection Info ===");
    Serial.printf("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    if (isConnected()) {
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP Address: %s\n", getIPAddress().c_str());
        Serial.printf("Signal Strength: %d dBm\n", getSignalStrength());
        Serial.printf("MAC Address: %s\n", getMACAddress().c_str());
        Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    }
    Serial.println("============================");
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
        Serial.printf("AP Mode started: %s\n", ssid);
        Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    
    return success;
}

void WiFiManager::stopAPMode() {
    WiFi.softAPdisconnect(true);
    Serial.println("AP Mode stopped");
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
        
        Serial.println("Start OTA updating " + type);
        webConfig.setOTAInProgress(true);  // Suspend main loop operations
        displayManager.showOTAProgress("ArduinoOTA Update", 0, "Starting...");
        
        otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Starting OTA update...");
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update Complete");
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
            Serial.printf("OTA Progress: %u%%\n", percent);
            otaManager.setProgress(percent);
            lastPercent = percent;
        }
        systemMonitor.forceResetWatchdog();
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        String errorMsg = "";
        
        if (error == OTA_AUTH_ERROR) {
            errorMsg = "Auth Failed";
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            errorMsg = "Begin Failed";
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            errorMsg = "Connect Failed";
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            errorMsg = "Receive Failed";
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            errorMsg = "End Failed";
            Serial.println("End Failed");
        }
        
        displayManager.showOTAProgress("OTA Error", 0, errorMsg.c_str());
        delay(3000);
        
        webConfig.setOTAInProgress(false);  // Resume main loop operations
        otaManager.setStatus(OTA_UPDATE_FAILED, errorMsg.c_str());
    });
    
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA initialized");
    if (debugPrintFunc) debugPrintFunc("ArduinoOTA ready", COLOR_GREEN);
}

void WiFiManager::handleOTA() {
    ArduinoOTA.handle();
}
