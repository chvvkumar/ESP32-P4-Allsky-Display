#include "mqtt_manager.h"
#include "system_monitor.h"
#include "display_manager.h"
#include "config_storage.h"
#include "device_health.h"
#include "logging.h"

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
    LOG_DEBUG("[MQTT] Initializing MQTT manager");
    
    // Configure MQTT client
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(messageCallback);
    mqttClient.setBufferSize(2048); // Increase buffer size for Home Assistant discovery messages
    LOG_DEBUG_F("[MQTT] Buffer size set to: %d bytes\n", 2048);
    
    // Initialize Home Assistant discovery
    LOG_DEBUG("[MQTT] Initializing Home Assistant discovery");
    haDiscovery.begin(&mqttClient);
    
    LOG_DEBUG("[MQTT] Manager initialization complete");
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
    
    LOG_DEBUG("[MQTT] ===== Connection Attempt =====");
    LOG_DEBUG_F("[MQTT] Server: %s:%d\n", configStorage.getMQTTServer().c_str(), configStorage.getMQTTPort());
    LOG_DEBUG_F("[MQTT] Client ID: %s\n", clientId.c_str());
    
    // Set up Last Will and Testament (LWT) for Home Assistant availability
    String availabilityTopic = haDiscovery.getAvailabilityTopic();
    LOG_DEBUG_F("[MQTT] LWT Topic: %s\n", availabilityTopic.c_str());
    
    bool connected = false;
    String mqttUser = configStorage.getMQTTUser();
    String mqttPassword = configStorage.getMQTTPassword();
    
    if (mqttUser.length() > 0) {
        LOG_DEBUG_F("[MQTT] Using authentication (username: %s)\n", mqttUser.c_str());
        connected = mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPassword.c_str(),
                                      availabilityTopic.c_str(), 1, true, "offline");
    } else {
        LOG_DEBUG("[MQTT] Connecting without authentication");
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
        
        LOG_INFO_F("✓ MQTT connected to %s\n", configStorage.getMQTTServer().c_str());
        LOG_DEBUG_F("[MQTT] Max packet size: %d bytes\n", mqttClient.getBufferSize());
        
        // Publish availability as online
        esp_task_wdt_reset();
        LOG_DEBUG("[MQTT] Publishing availability: online");
        haDiscovery.publishAvailability(true);
        
        // Publish Home Assistant discovery if enabled
        if (configStorage.getHADiscoveryEnabled()) {
            esp_task_wdt_reset();
            LOG_DEBUG("[MQTT] Home Assistant discovery enabled, publishing...");
            
            if (haDiscovery.publishDiscovery()) {
                discoveryPublished = true;
                LOG_DEBUG("[MQTT] ✓ HA discovery messages published");
                
                // Subscribe to command topic filter
                String commandFilter = haDiscovery.getCommandTopicFilter();
                LOG_DEBUG_F("[MQTT] Subscribing to HA commands: %s\n", commandFilter.c_str());
                if (!mqttClient.subscribe(commandFilter.c_str())) {
                    LOG_ERROR_F("[MQTT] ✗ FAILED to subscribe to HA command topics! MQTT state: %d\n", mqttClient.state());
                } else {
                    LOG_DEBUG("[MQTT] ✓ Subscribed to HA command topics");
                }
                
                // Publish initial state
                esp_task_wdt_reset();
                LOG_DEBUG("[MQTT] Publishing initial state to HA");
                haDiscovery.publishState();
            } else {
                LOG_WARNING("[MQTT] Failed to publish HA discovery");
            }
        } else {
            LOG_DEBUG("[MQTT] HA discovery disabled in configuration");
        }
        
    } else {
        mqttConnected = false;
        reconnectFailures++;
        
        // Implement exponential backoff for failed connections
        if (reconnectFailures > 3) {
            reconnectBackoff = min((unsigned long)(reconnectBackoff * 1.5), 60000UL);  // Max 1 minute
        }
        
        int mqttState = mqttClient.state();
        LOG_ERROR_F("[MQTT] ✗ Connection failed! State code: %d (attempt #%d)\n", mqttState, reconnectFailures);
        logPrint("[MQTT] Error meaning: ", LOG_INFO);
        switch(mqttState) {
            case -4: LOG_INFO("MQTT_CONNECTION_TIMEOUT"); break;
            case -3: LOG_INFO("MQTT_CONNECTION_LOST"); break;
            case -2: LOG_ERROR("MQTT_CONNECT_FAILED - Network error"); break;
            case -1: LOG_INFO("MQTT_DISCONNECTED"); break;
            case 1: LOG_INFO("MQTT_CONNECT_BAD_PROTOCOL"); break;
            case 2: LOG_INFO("MQTT_CONNECT_BAD_CLIENT_ID"); break;
            case 3: LOG_INFO("MQTT_CONNECT_UNAVAILABLE - Server unavailable"); break;
            case 4: LOG_INFO("MQTT_CONNECT_BAD_CREDENTIALS - Check username/password"); break;
            case 5: LOG_INFO("MQTT_CONNECT_UNAUTHORIZED - Not authorized"); break;
            default: LOG_ERROR("Unknown error code"); break;
        }
        LOG_INFO_F("[MQTT] Next retry in %lu ms (backoff: %lu ms)\n", reconnectBackoff, reconnectBackoff);
        
        if (debugPrintFunc && !firstImageLoaded) {
            debugPrintfFunc(COLOR_RED, "MQTT failed, state: %d", mqttState);
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
        
        // Track reconnection attempt for health monitoring
        DeviceHealthAnalyzer::recordMQTTReconnect();
        
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
    
    LOG_DEBUG_F("[MQTT] Message received on topic: %s\n", topic);
    LOG_DEBUG_F("[MQTT] Message payload: %s\n", message.c_str());
    
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
        if (currentConnectionState) {
            LOG_INFO("[MQTT] State change: Disconnected -> Connected");
        } else {
            LOG_WARNING("[MQTT] State change: Connected -> Disconnected");
        }
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
    LOG_INFO("=== MQTT Connection Info ===");
    LOG_INFO_F("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    LOG_INFO_F("Server: %s:%d\n", configStorage.getMQTTServer().c_str(), configStorage.getMQTTPort());
    LOG_INFO_F("Client ID: %s\n", configStorage.getMQTTClientID().c_str());
    if (isConnected()) {
        if (configStorage.getHADiscoveryEnabled()) {
            LOG_INFO("Home Assistant Discovery: Enabled");
            LOG_INFO_F("Device Name: %s\n", configStorage.getHADeviceName().c_str());
            LOG_INFO_F("Base Topic: %s\n", haDiscovery.getCommandTopicFilter().c_str());
        } else {
            LOG_INFO("Home Assistant Discovery: Disabled");
        }
    }
    LOG_INFO("============================");
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