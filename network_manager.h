#pragma once
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiManager {
private:
    bool wifiConnected;
    unsigned long lastConnectionAttempt;
    int connectionAttempts;
    
    // Debug display function pointer
    void (*debugPrintFunc)(const char* message, uint16_t color);
    void (*debugPrintfFunc)(uint16_t color, const char* format, ...);
    bool firstImageLoaded;

public:
    WiFiManager();
    
    // Initialization
    bool begin();
    
    // WiFi management
    void connectToWiFi();
    bool isConnected() const;
    void checkConnection();
    
    // Status information
    String getIPAddress() const;
    String getMACAddress() const;
    int getSignalStrength() const;
    
    // Debug functions
    void setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                          void (*debugPrintf)(uint16_t, const char*, ...),
                          bool* firstImageLoadedFlag);
    
    // Update function - call this in main loop
    void update();
    
    // Time synchronization
    void syncNTPTime();
    bool isTimeValid();
    
    // Print connection info
    void printConnectionInfo();
    
    // AP mode support
    bool startAPMode(const char* ssid, const char* password = nullptr);
    void stopAPMode();
    bool isAPMode() const;
    
    // ArduinoOTA support
    void initOTA();
    void handleOTA();
    
    // WiFi scanning
    int scanNetworks(bool async = false, bool show_hidden = true);
    String getScanResultsJSON();
    bool isScanComplete();
    static unsigned long lastScanTime;
};


// WiFi scan result structure
struct WiFiScanResult {
    String ssid;
    int32_t rssi;
    uint8_t encryption;
    uint8_t channel;
    bool is_2_4GHz;
};

// Global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
