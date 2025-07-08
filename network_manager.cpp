#include "network_manager.h"
#include "system_monitor.h"

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
    // Validate WiFi credentials first
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("ERROR: WiFi SSID is empty!");
        if (debugPrintFunc) debugPrintFunc("ERROR: WiFi SSID is empty!", COLOR_RED);
        return false;
    }
    
    Serial.println("Setting WiFi mode to STA");
    if (debugPrintFunc) debugPrintFunc("Setting WiFi mode to STA", COLOR_WHITE);
    
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
    
    if (debugPrintFunc) {
        debugPrintfFunc(COLOR_WHITE, "Connecting to: %s", WIFI_SSID);
        debugPrintFunc("WiFi connecting", COLOR_YELLOW);
    }
    
    Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
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
        
        // Show progress dots
        if (!firstImageLoaded && debugPrintFunc) {
            Serial.print(".");
        }
        
        if (connectionAttempts % 4 == 0) {  // More frequent status updates
            if (debugPrintfFunc) {
                debugPrintfFunc(COLOR_YELLOW, "Attempt %d/%d...", connectionAttempts, WIFI_MAX_ATTEMPTS);
            }
            systemMonitor.resetWatchdog(); // Additional watchdog reset
        }
        
        // Allow other tasks to run
        yield();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi connected!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal: %d dBm\n", WiFi.RSSI());
        Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
        
        if (debugPrintFunc) {
            debugPrintFunc("WiFi connected!", COLOR_GREEN);
            debugPrintfFunc(COLOR_GREEN, "IP: %s", WiFi.localIP().toString().c_str());
            debugPrintfFunc(COLOR_WHITE, "Signal: %d dBm", WiFi.RSSI());
            debugPrintfFunc(COLOR_WHITE, "MAC: %s", WiFi.macAddress().c_str());
        }
    } else {
        wifiConnected = false;
        Serial.printf("WiFi failed after %d attempts\n", connectionAttempts);
        Serial.printf("Status code: %d\n", WiFi.status());
        Serial.println("Will retry in main loop...");
        
        if (debugPrintFunc) {
            debugPrintfFunc(COLOR_RED, "WiFi failed after %d attempts", connectionAttempts);
            debugPrintfFunc(COLOR_RED, "Status code: %d", WiFi.status());
            debugPrintFunc("Will retry in main loop...", COLOR_YELLOW);
        }
        
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
        if (currentStatus) {
            wifiConnected = true;
            Serial.println("WiFi reconnected!");
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintFunc("WiFi reconnected!", COLOR_GREEN);
            }
        } else {
            wifiConnected = false;
            Serial.println("WiFi disconnected!");
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintFunc("ERROR: WiFi disconnected!", COLOR_RED);
                debugPrintFunc("Attempting reconnection...", COLOR_YELLOW);
            }
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
