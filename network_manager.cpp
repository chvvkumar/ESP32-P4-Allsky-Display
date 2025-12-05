#include "network_manager.h"
#include "system_monitor.h"

// Global instance
WiFiManager wifiManager;

WiFiManager::WiFiManager() :
    wifiConnected(false),
    isAccessPoint(false),
    lastConnectionAttempt(0),
    connectionAttempts(0),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false)
{
}

bool WiFiManager::begin() {
    // WiFi manager initialized successfully
    // Mode will be set later based on configuration status
    LOG_PRINTLN("WiFi Manager initialized");
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
        LOG_PRINTF("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifiConnected = false;
        LOG_PRINTF("WiFi failed (status: %d)\n", WiFi.status());
        
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
            LOG_PRINTLN("WiFi disconnected!");
        }
    }
}

String WiFiManager::getIPAddress() const {
    if (isAccessPoint) {
        return WiFi.softAPIP().toString();
    }
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
    // Don't try to reconnect if we're in AP mode
    if (isAccessPoint) {
        return;
    }
    
    checkConnection();
    
    // Attempt reconnection if disconnected
    if (!isConnected()) {
        connectToWiFi();
    }
}

bool WiFiManager::startConfigPortal() {
    LOG_PRINTLN("Starting WiFi Configuration Portal...");
    
    // Stop any existing WiFi connection
    WiFi.disconnect();
    delay(100);
    
    // Start Access Point
    String apName = "ESP32-AllSky-" + WiFi.macAddress().substring(9);
    apName.replace(":", "");
    
    LOG_PRINTF("AP Name: %s\n", apName.c_str());
    
    if (!WiFi.softAP(apName.c_str())) {
        LOG_PRINTLN("ERROR: Failed to start Access Point!");
        if (debugPrintFunc) debugPrintFunc("ERROR: Failed to start AP", COLOR_RED);
        return false;
    }
    
    isAccessPoint = true;
    wifiConnected = false;
    
    // Get AP IP address
    IPAddress apIP = WiFi.softAPIP();
    
    LOG_PRINTF("AP Started! IP: %s\n", apIP.toString().c_str());
    
    if (debugPrintFunc) {
        debugPrintFunc("WiFi Config Portal Active", COLOR_CYAN);
        debugPrintfFunc(COLOR_WHITE, "AP: %s", apName.c_str());
        debugPrintfFunc(COLOR_WHITE, "IP: %s", apIP.toString().c_str());
        debugPrintFunc("Connect and visit IP to configure", COLOR_YELLOW);
    }
    
    return true;
}

bool WiFiManager::isInAPMode() const {
    return isAccessPoint;
}

void WiFiManager::printConnectionInfo() {
    LOG_PRINTLN("=== WiFi Connection Info ===");
    
    if (isAccessPoint) {
        LOG_PRINTLN("Mode: Access Point (Config Portal)");
        LOG_PRINTF("AP SSID: %s\n", WiFi.softAPSSID().c_str());
        LOG_PRINTF("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        LOG_PRINTF("Connected Clients: %d\n", WiFi.softAPgetStationNum());
    } else {
        LOG_PRINTF("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
        if (isConnected()) {
            LOG_PRINTF("SSID: %s\n", WiFi.SSID().c_str());
            LOG_PRINTF("IP Address: %s\n", getIPAddress().c_str());
            LOG_PRINTF("Signal Strength: %d dBm\n", getSignalStrength());
            LOG_PRINTF("MAC Address: %s\n", getMACAddress().c_str());
            LOG_PRINTF("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            LOG_PRINTF("DNS: %s\n", WiFi.dnsIP().toString().c_str());
        }
    }
    LOG_PRINTLN("============================");
}
