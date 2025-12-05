#include "mqtt_manager.h"
#include "system_monitor.h"
#include "display_manager.h"
#include "config_storage.h"

// Global instance
MQTTManager mqttManager;

MQTTManager::MQTTManager() :
    mqttClient(wifiClient),
    mqttConnected(false),
    lastReconnectAttempt(0),
    reconnectBackoff(5000),
    reconnectFailures(0),
    discoveryPublished(false),
    lastAvailabilityPublish(0),
    lastSensorPublish(0),
    lastStatusLog(0),
    lastConnectionState(false),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false)
{
}

bool MQTTManager::begin() {
    // Configure MQTT client
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(messageCallback);
    mqttClient.setBufferSize(2048); // Increase buffer size for Home Assistant discovery messages
    
    // Initialize Home Assistant discovery
    haDiscovery.begin(&mqttClient);
    
    return true;
}

void MQTTManager::connect() {
    if (mqttConnected) return;
    
    // Reset watchdog before attempting connection
    esp_task_wdt_reset();
    
    // Generate unique client ID
    String clientId = String(configStorage.getMQTTClientID().c_str()) + "_" + String(random(0xffff), HEX);
    
    // Set connection timeout to prevent blocking
    mqttClient.setSocketTimeout(2);  // 2 second timeout
    
    // Reset watchdog before connection attempt
    esp_task_wdt_reset();
    
    // Set up Last Will and Testament (LWT) for Home Assistant availability
    String availabilityTopic = haDiscovery.getAvailabilityTopic();
    
    bool connected = false;
    String mqttUser = configStorage.getMQTTUser();
    String mqttPassword = configStorage.getMQTTPassword();
    
    if (mqttUser.length() > 0) {
        connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str(),
                                      availabilityTopic.c_str(), 0, true, "offline");
    } else {
        connected = mqttClient.connect(clientId.c_str(), nullptr, nullptr,
                                      availabilityTopic.c_str(), 0, true, "offline");
    }
    
    // Reset watchdog after connection attempt
    esp_task_wdt_reset();
    
    if (connected) {
        mqttConnected = true;
        reconnectFailures = 0;  // Reset failure counter
        reconnectBackoff = 5000;  // Reset backoff
        discoveryPublished = false; // Reset discovery flag
        
        // Publish availability as online
        esp_task_wdt_reset();
        haDiscovery.publishAvailability(true);
        
        // Publish Home Assistant discovery if enabled
        if (configStorage.getHADiscoveryEnabled()) {
            esp_task_wdt_reset();
            
            if (haDiscovery.publishDiscovery()) {
                discoveryPublished = true;
                
                // Subscribe to command topic filter
                String commandFilter = haDiscovery.getCommandTopicFilter();
                if (!mqttClient.subscribe(commandFilter.c_str())) {
                    LOG_PRINTF("✗ FAILED to subscribe to HA command topics! MQTT state: %d\n", mqttClient.state());
                }
                
                // Publish initial state
                esp_task_wdt_reset();
                haDiscovery.publishState();
            }
        }
        
    } else {
        mqttConnected = false;
        reconnectFailures++;
        
        // Implement exponential backoff for failed connections
        if (reconnectFailures > 3) {
            reconnectBackoff = min((unsigned long)(reconnectBackoff * 1.5), 60000UL);  // Max 1 minute
        }
        
        LOG_PRINTF("MQTT connection failed, state: %d (attempt %d)\n", mqttClient.state(), reconnectFailures);
        if (debugPrintFunc && !firstImageLoaded) {
            debugPrintfFunc(COLOR_RED, "MQTT failed, state: %d", mqttClient.state());
        }
    }
    
    // Final watchdog reset
    esp_task_wdt_reset();
}

bool MQTTManager::isConnected() {
    return mqttConnected && mqttClient.connected();
}

void MQTTManager::reconnect() {
    unsigned long now = millis();
    
    // Use dynamic backoff interval instead of fixed MQTT_RECONNECT_INTERVAL
    unsigned long interval = max(reconnectBackoff, (unsigned long)MQTT_RECONNECT_INTERVAL);
    
    if (now - lastReconnectAttempt > interval) {
        lastReconnectAttempt = now;
        
        // Reset watchdog before reconnect attempt
        esp_task_wdt_reset();
        
        connect();
        
        // Reset watchdog after reconnect attempt
        esp_task_wdt_reset();
    }
}

void MQTTManager::loop() {
    if (mqttClient.connected()) {
        mqttClient.loop();
    }
}

void MQTTManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    // Handle Home Assistant commands
    String topicStr = String(topic);
    haDiscovery.handleCommand(topicStr, message);
}


void MQTTManager::setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                                   void (*debugPrintf)(uint16_t, const char*, ...),
                                   bool* firstImageLoadedFlag) {
    debugPrintFunc = debugPrint;
    debugPrintfFunc = debugPrintf;
    if (firstImageLoadedFlag) {
        firstImageLoaded = *firstImageLoadedFlag;
    }
}

void MQTTManager::update() {
    bool currentConnectionState = isConnected();
    
    // Track connection state changes
    if (currentConnectionState != lastConnectionState) {
        lastConnectionState = currentConnectionState;
    }
    
    if (!currentConnectionState) {
        if (mqttConnected) {
            mqttConnected = false;
        }
        reconnect();
    } else {
        loop();
        
        // Publish availability heartbeat every 30 seconds
        publishAvailabilityHeartbeat();
        
        // Update Home Assistant discovery (publishes sensor updates)
        haDiscovery.update();
    }
    
    // Log connection status every 30 seconds
    logConnectionStatus();
}

void MQTTManager::printConnectionInfo() {
    LOG_PRINTLN("=== MQTT Connection Info ===");
    LOG_PRINTF("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    LOG_PRINTF("Server: %s:%d\n", configStorage.getMQTTServer().c_str(), configStorage.getMQTTPort());
    LOG_PRINTF("Client ID: %s\n", configStorage.getMQTTClientID().c_str());
    if (isConnected()) {
        if (configStorage.getHADiscoveryEnabled()) {
            LOG_PRINTLN("Home Assistant Discovery: Enabled");
            LOG_PRINTF("Device Name: %s\n", configStorage.getHADeviceName().c_str());
            LOG_PRINTF("Base Topic: %s\n", haDiscovery.getCommandTopicFilter().c_str());
        } else {
            LOG_PRINTLN("Home Assistant Discovery: Disabled");
        }
    }
    LOG_PRINTLN("============================");
}

void MQTTManager::logConnectionStatus() {
    // Periodic status logging removed to reduce code size
}

void MQTTManager::publishAvailabilityHeartbeat() {
    if (!isConnected()) {
        return;
    }
    
    unsigned long now = millis();
    
    // Publish availability heartbeat every 30 seconds
    if (now - lastAvailabilityPublish >= 30000) {
        lastAvailabilityPublish = now;
        haDiscovery.publishAvailability(true);
    }
}

PubSubClient* MQTTManager::getClient() {
    return &mqttClient;
}
