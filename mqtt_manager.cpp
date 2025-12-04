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
    
    Serial.println("MQTT Manager initialized");
    return true;
}

void MQTTManager::connect() {
    if (mqttConnected) return;
    
    // Reset watchdog before attempting connection
    esp_task_wdt_reset();
    
    if (debugPrintFunc && !firstImageLoaded) {
        debugPrintFunc("Connecting to MQTT...", COLOR_YELLOW);
        debugPrintfFunc(COLOR_WHITE, "Broker: %s:%d", MQTT_SERVER, MQTT_PORT);
    }
    
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
        
        Serial.printf("MQTT connected as: %s\n", clientId.c_str());
        
        if (debugPrintFunc && !firstImageLoaded) {
            debugPrintFunc("MQTT connected!", COLOR_GREEN);
        }
        
        // Publish availability as online
        esp_task_wdt_reset();
        haDiscovery.publishAvailability(true);
        
        // Publish Home Assistant discovery if enabled
        if (configStorage.getHADiscoveryEnabled()) {
            esp_task_wdt_reset();
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintFunc("Publishing HA discovery...", COLOR_CYAN);
            }
            
            if (haDiscovery.publishDiscovery()) {
                discoveryPublished = true;
                Serial.println("Home Assistant discovery published");
                
                // Subscribe to command topic filter
                String commandFilter = haDiscovery.getCommandTopicFilter();
                Serial.printf("Attempting to subscribe to: '%s'\n", commandFilter.c_str());
                if (mqttClient.subscribe(commandFilter.c_str())) {
                    Serial.printf("✓ Successfully subscribed to HA commands: %s\n", commandFilter.c_str());
                    if (debugPrintFunc && !firstImageLoaded) {
                        debugPrintFunc("HA discovery complete!", COLOR_GREEN);
                    }
                } else {
                    Serial.printf("✗ FAILED to subscribe to HA command topics! MQTT state: %d\n", mqttClient.state());
                }
                
                // Publish initial state
                esp_task_wdt_reset();
                haDiscovery.publishState();
            } else {
                Serial.println("Failed to publish HA discovery");
            }
        }
        
    } else {
        mqttConnected = false;
        reconnectFailures++;
        
        // Implement exponential backoff for failed connections
        if (reconnectFailures > 3) {
            reconnectBackoff = min((unsigned long)(reconnectBackoff * 1.5), 60000UL);  // Max 1 minute
        }
        
        Serial.printf("MQTT connection failed, state: %d (attempt %d)\n", mqttClient.state(), reconnectFailures);
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
    
    Serial.println("============================================================");
    Serial.println("MQTT MESSAGE RECEIVED:");
    Serial.printf("  Topic: '%s'\n", topic);
    Serial.printf("  Payload: '%s' (length: %d)\n", message.c_str(), length);
    Serial.println("============================================================");
    
    // Handle Home Assistant commands
    String topicStr = String(topic);
    haDiscovery.handleCommand(topicStr, message);
    Serial.println("Command handled.\n");
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
    
    // Log connection state changes immediately
    if (currentConnectionState != lastConnectionState) {
        lastConnectionState = currentConnectionState;
        if (currentConnectionState) {
            Serial.println("MQTT: Connection established");
        } else {
            Serial.println("MQTT: Connection lost");
        }
    }
    
    if (!currentConnectionState) {
        if (mqttConnected) {
            mqttConnected = false;
            Serial.println("MQTT: Disconnected - attempting reconnect");
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
    Serial.println("=== MQTT Connection Info ===");
    Serial.printf("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    Serial.printf("Server: %s:%d\n", configStorage.getMQTTServer().c_str(), configStorage.getMQTTPort());
    Serial.printf("Client ID: %s\n", configStorage.getMQTTClientID().c_str());
    if (isConnected()) {
        if (configStorage.getHADiscoveryEnabled()) {
            Serial.println("Home Assistant Discovery: Enabled");
            Serial.printf("Device Name: %s\n", configStorage.getHADeviceName().c_str());
            Serial.printf("Base Topic: %s\n", haDiscovery.getCommandTopicFilter().c_str());
        } else {
            Serial.println("Home Assistant Discovery: Disabled");
        }
    }
    Serial.println("============================");
}

void MQTTManager::logConnectionStatus() {
    unsigned long now = millis();
    
    // Log status every 30 seconds
    if (now - lastStatusLog >= 30000) {
        lastStatusLog = now;
        
        Serial.println("=== MQTT Status ===");
        Serial.printf("Connected: %s\n", isConnected() ? "YES" : "NO");
        
        if (isConnected()) {
            Serial.printf("Broker: %s:%d\n", configStorage.getMQTTServer().c_str(), configStorage.getMQTTPort());
            Serial.printf("Discovery Published: %s\n", discoveryPublished ? "YES" : "NO");
            
            unsigned long timeSinceAvailability = (lastAvailabilityPublish > 0) ? (now - lastAvailabilityPublish) : 0;
            
            // Get sensor publish time from HA discovery
            unsigned long haSensorPublish = haDiscovery.getLastSensorPublish();
            unsigned long timeSinceSensor = (haSensorPublish > 0) ? (now - haSensorPublish) : 0;
            
            Serial.printf("Last Availability: %lu s ago\n", timeSinceAvailability / 1000);
            Serial.printf("Last Sensor Update: %lu s ago\n", timeSinceSensor / 1000);
        } else {
            Serial.printf("Failed Attempts: %d\n", reconnectFailures);
            unsigned long nextRetryTime = (lastReconnectAttempt + reconnectBackoff > now) ? 
                                          (lastReconnectAttempt + reconnectBackoff - now) / 1000 : 0;
            Serial.printf("Next Retry: %lu s\n", nextRetryTime);
            Serial.printf("MQTT State: %d\n", mqttClient.state());
        }
        Serial.println("==================");
    }
}

void MQTTManager::publishAvailabilityHeartbeat() {
    if (!isConnected()) {
        return;
    }
    
    unsigned long now = millis();
    
    // Publish availability heartbeat every 30 seconds
    if (now - lastAvailabilityPublish >= 30000) {
        lastAvailabilityPublish = now;
        
        if (haDiscovery.publishAvailability(true)) {
            Serial.println("MQTT: Availability heartbeat sent");
        } else {
            Serial.println("MQTT: WARNING - Failed to send availability heartbeat");
        }
    }
}

PubSubClient* MQTTManager::getClient() {
    return &mqttClient;
}
