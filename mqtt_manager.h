#pragma once
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"

class MQTTManager {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    bool mqttConnected;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectBackoff;
    int reconnectFailures;
    
    // Debug function pointers
    void (*debugPrintFunc)(const char* message, uint16_t color);
    void (*debugPrintfFunc)(uint16_t color, const char* format, ...);
    bool firstImageLoaded;

public:
    MQTTManager();
    
    // Initialization
    bool begin();
    
    // Connection management
    void connect();
    bool isConnected();
    void reconnect();
    
    // Message handling
    void loop();
    static void messageCallback(char* topic, byte* payload, unsigned int length);
    
    // Publishing
    bool publishBrightnessStatus(int brightness);
    
    // Debug functions
    void setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                          void (*debugPrintf)(uint16_t, const char*, ...),
                          bool* firstImageLoadedFlag);
    
    // Update function - call this in main loop
    void update();
    
    // Status information
    void printConnectionInfo();
    
private:
    void handleRebootMessage(const String& message);
    void handleBrightnessMessage(const String& message);
};

// Global instance
extern MQTTManager mqttManager;

#endif // MQTT_MANAGER_H
