#include "network_manager.h"
#include "system_monitor.h"
#include "display_manager.h"
#include "ota_manager.h"
#include <ArduinoOTA.h>

// Global instance
WiFiManager wifiManager;

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
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    connectionAttempts = 0;
    unsigned long startTime = millis();
    
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < WIFI_MAX_ATTEMPTS) {
        // Check for timeout to prevent infinite hanging
        if (millis() - startTime > WIFI_MAX_WAIT_TIME) {
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
        Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifiConnected = false;
        Serial.printf("WiFi failed (status: %d)\n", WiFi.status());
        
        // Disconnect to clean up any partial connection state
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
            Serial.println("WiFi disconnected!");
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
        displayManager.debugPrint(("OTA Update: " + type).c_str(), COLOR_YELLOW);
        
        // Pause display during OTA
        displayManager.pauseDisplay();
        
        otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Starting OTA update...");
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update Complete");
        displayManager.debugPrint("OTA Complete! Rebooting...", COLOR_GREEN);
        
        otaManager.setStatus(OTA_UPDATE_SUCCESS, "OTA update successful");
        
        // Resume display
        displayManager.resumeDisplay();
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        uint8_t percent = (progress * 100) / total;
        
        // Update progress every 10%
        static uint8_t lastPercent = 0;
        if (percent != lastPercent && percent % 10 == 0) {
            Serial.printf("OTA Progress: %u%%\n", percent);
            
            char msg[64];
            snprintf(msg, sizeof(msg), "OTA Progress: %u%%", percent);
            displayManager.debugPrint(msg, COLOR_CYAN);
            
            otaManager.setProgress(percent);
            lastPercent = percent;
        }
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        String errorMsg = "OTA Error: ";
        
        if (error == OTA_AUTH_ERROR) {
            errorMsg += "Auth Failed";
            Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
            errorMsg += "Begin Failed";
            Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
            errorMsg += "Connect Failed";
            Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            errorMsg += "Receive Failed";
            Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
            errorMsg += "End Failed";
            Serial.println("End Failed");
        }
        
        displayManager.debugPrint(errorMsg.c_str(), COLOR_RED);
        otaManager.setStatus(OTA_UPDATE_FAILED, errorMsg.c_str());
        
        // Resume display
        displayManager.resumeDisplay();
    });
    
    ArduinoOTA.begin();
    Serial.println("ArduinoOTA initialized");
    if (debugPrintFunc) debugPrintFunc("ArduinoOTA ready", COLOR_GREEN);
}

void WiFiManager::handleOTA() {
    ArduinoOTA.handle();
}
