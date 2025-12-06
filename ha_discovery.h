#pragma once
#ifndef HA_DISCOVERY_H
#define HA_DISCOVERY_H

#include <Arduino.h>
#include <PubSubClient.h>
#include "config_storage.h"

class HADiscovery {
private:
    PubSubClient* mqttClient;
    String deviceId;  // Unique device ID based on MAC address
    String baseTopic; // Base topic for all device entities
    unsigned long lastSensorUpdate;
    unsigned long lastSensorPublish;
    
    // Helper methods for discovery message generation
    String getDeviceId();
    String getBaseTopic();
    String buildDiscoveryTopic(const char* component, const char* entityId);
    String buildStateTopic(const char* entity = nullptr);
    String buildCommandTopic(const char* entity);
    String buildAttributesTopic();
    
    // Device information JSON
    String getDeviceJson();
    
    // Discovery message builders for each entity type
    bool publishLightDiscovery();
    bool publishSwitchDiscovery(const char* entityId, const char* name, const char* icon);
    bool publishNumberDiscovery(const char* entityId, const char* name, float min, float max, float step, const char* unit, const char* icon);
    bool publishSelectDiscovery();
    bool publishButtonDiscovery(const char* entityId, const char* name, const char* icon);
    bool publishSensorDiscovery(const char* entityId, const char* name, const char* unit, const char* deviceClass, const char* icon);

public:
    HADiscovery();
    
    // Initialize with MQTT client
    void begin(PubSubClient* client);
    
    // Publish all discovery messages
    bool publishDiscovery();
    
    // Publish availability status
    bool publishAvailability(bool online);
    
    // Publish device state (all entities)
    bool publishState();
    
    // Publish sensor updates only
    bool publishSensors();
    
    // Update loop - call regularly to publish sensor updates
    void update();
    
    // Get last sensor publish time
    unsigned long getLastSensorPublish() const { return lastSensorPublish; }
    
    // Handle incoming command messages
    void handleCommand(const String& topic, const String& payload);
    
    // Get the base command topic for subscription
    String getCommandTopicFilter();
    
    // Get the availability topic
    String getAvailabilityTopic();
};

// Global instance
extern HADiscovery haDiscovery;

#endif // HA_DISCOVERY_H
