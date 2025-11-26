#pragma once
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "config.h"
#include "ha_discovery.h"

class MQTTManager {
private:
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    bool mqttConnected;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectBackoff;
    int reconnectFailures;
    bool discoveryPublished;
    
    // Status tracking
    unsigned long lastAvailabilityPublish;
    unsigned long lastSensorPublish;
    unsigned long lastStatusLog;
    bool lastConnectionState;
    
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
    
    // Debug functions
    void setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                          void (*debugPrintf)(uint16_t, const char*, ...),
                          bool* firstImageLoadedFlag);
    
    // Update function - call this in main loop
    void update();
    
    // Status information
    void printConnectionInfo();
    void logConnectionStatus();
    
    // Heartbeat
    void publishAvailabilityHeartbeat();
    
    // Get MQTT client for HA discovery
    PubSubClient* getClient();
    
    // Get last publish times
    unsigned long getLastSensorPublish() const { return lastSensorPublish; }
    unsigned long getLastAvailabilityPublish() const { return lastAvailabilityPublish; }
};

// Global instance
extern MQTTManager mqttManager;

#endif // MQTT_MANAGER_H
