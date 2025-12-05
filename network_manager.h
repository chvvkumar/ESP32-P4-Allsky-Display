#pragma once
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

class WiFiManager {
private:
    bool wifiConnected;
    bool isAccessPoint;
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
    
    // Access Point mode for configuration
    bool startConfigPortal();
    bool isInAPMode() const;
    
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
    
    // Print connection info
    void printConnectionInfo();
};


// Global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
