#include "network_manager.h"
#include "system_monitor.h"

// Global instance
WiFiManager wifiManager;

WiFiManager::WiFiManager() :
    wifiConnected(false),
    lastConnectionAttempt(0),
    connectionAttempts(0),
    apModeEnabled(false),
    apStartupAttempted(false),
    apStartTime(0),
    failedConnectionAttempts(0),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false)
{
}

bool WiFiManager::begin() {
    // Check WiFi credentials - if empty, we'll start AP mode
    if (strlen(WIFI_SSID) == 0) {
        Serial.println("WARNING: WiFi SSID is empty - will start setup hotspot!");
        if (debugPrintFunc) debugPrintFunc("WiFi SSID empty - setup hotspot starting", COLOR_YELLOW);
        // Don't fail - we'll handle this in the main flow by starting AP mode
    }
    
    Serial.println("Setting WiFi mode to STA");
    if (debugPrintFunc) debugPrintFunc("Setting WiFi mode to STA", COLOR_WHITE);
    
    WiFi.mode(WIFI_STA);
    return true;  // Always succeed on initialization
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
    
    // Don't attempt reconnection if we're in AP mode (initial setup)
    if (apModeEnabled) {
        return;
    }
    
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

// ============================================================================
// AP MODE (Access Point) FUNCTIONS FOR INITIAL WIFI SETUP
// ============================================================================

void WiFiManager::startWiFiSetupHotspot() {
    // Called when WiFi connection fails multiple times
    // Enables AP mode to allow initial WiFi configuration via web interface
    
    if (apStartupAttempted) {
        Serial.println("AP mode startup already attempted, skipping...");
        return;
    }
    
    apStartupAttempted = true;
    
    Serial.println("\n=== STARTING WIFI SETUP HOTSPOT ===");
    Serial.printf("SSID: %s\n", WIFI_AP_SSID);
    Serial.println("Password: Open network (no password required)");
    Serial.println("Access point IP: 192.168.4.1");
    Serial.println("Web config URL: http://192.168.4.1:8080");
    
    if (debugPrintFunc) {
        debugPrintFunc("=== WIFI SETUP HOTSPOT ===", COLOR_MAGENTA);
        debugPrintfFunc(COLOR_MAGENTA, "SSID: %s", WIFI_AP_SSID);
        debugPrintfFunc(COLOR_CYAN, "IP: 192.168.4.1:8080");
        debugPrintfFunc(COLOR_YELLOW, "No password required!");
    }
    
    enableAPMode();
}

void WiFiManager::enableAPMode() {
    // Enable AP mode and allow clients to connect
    
    if (apModeEnabled) {
        Serial.println("AP mode already enabled");
        return;
    }
    
    Serial.println("Switching WiFi to AP mode...");
    
    // Switch to AP mode
    WiFi.mode(WIFI_AP);
    
    // Start AP with configured SSID and password
    bool apSuccessful = false;
    
    if (strlen(WIFI_AP_PASSWORD) > 0) {
        // Start AP with password
        apSuccessful = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, WIFI_AP_MAX_CONNECTIONS);
    } else {
        // Start open AP (no password)
        apSuccessful = WiFi.softAP(WIFI_AP_SSID, "", 1, 0, WIFI_AP_MAX_CONNECTIONS);
    }
    
    if (apSuccessful) {
        apModeEnabled = true;
        apStartTime = millis();
        
        IPAddress apIP = WiFi.softAPIP();
        Serial.printf("AP mode enabled successfully!\n");
        Serial.printf("AP IP: %s\n", apIP.toString().c_str());
        Serial.printf("AP SSID: %s\n", WIFI_AP_SSID);
        Serial.printf("Max connections: %d\n", WIFI_AP_MAX_CONNECTIONS);
        Serial.printf("AP timeout: %lu ms (%lu minutes)\n", WIFI_AP_TIMEOUT, WIFI_AP_TIMEOUT / 60000);
        
        if (debugPrintFunc) {
            debugPrintfFunc(COLOR_GREEN, "AP mode enabled!");
            debugPrintfFunc(COLOR_CYAN, "AP IP: %s", apIP.toString().c_str());
        }
    } else {
        apModeEnabled = false;
        Serial.println("ERROR: Failed to enable AP mode!");
        
        if (debugPrintFunc) {
            debugPrintfFunc(COLOR_RED, "Failed to enable AP mode");
        }
    }
}

void WiFiManager::disableAPMode() {
    // Disable AP mode and switch back to STA mode
    
    if (!apModeEnabled) {
        Serial.println("AP mode not currently enabled");
        return;
    }
    
    Serial.println("Disabling AP mode and switching to STA...");
    
    WiFi.softAPdisconnect(true);  // Disconnect and turn off AP
    apModeEnabled = false;
    apStartupAttempted = false;
    
    // Return to STA mode
    WiFi.mode(WIFI_STA);
    
    Serial.println("Switched back to WiFi STA mode");
    
    if (debugPrintFunc) {
        debugPrintfFunc(COLOR_YELLOW, "Switched to STA mode");
    }
}

bool WiFiManager::isAPModeEnabled() const {
    return apModeEnabled;
}

void WiFiManager::updateAPMode() {
    // Update AP mode status and check for timeouts
    
    if (!apModeEnabled) {
        return;
    }
    
    // Check if AP timeout has been exceeded
    checkAPTimeout();
}

void WiFiManager::checkAPTimeout() {
    // Check if AP mode has exceeded its timeout duration
    // Auto-disable AP mode if timeout is reached
    
    if (!apModeEnabled || apStartTime == 0) {
        return;
    }
    
    unsigned long apRunTime = millis() - apStartTime;
    
    if (apRunTime > WIFI_AP_TIMEOUT) {
        Serial.println("AP mode timeout reached, disabling AP mode");
        Serial.println("Device will attempt to connect to configured WiFi");
        
        if (debugPrintFunc) {
            debugPrintfFunc(COLOR_YELLOW, "AP timeout - switching to STA");
        }
        
        disableAPMode();
    }
}
