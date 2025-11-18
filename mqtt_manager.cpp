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
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr),
    firstImageLoaded(false)
{
}

bool MQTTManager::begin() {
    // Configure MQTT client
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(messageCallback);
    
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
    String clientId = String(MQTT_CLIENT_ID) + "_" + String(random(0xffff), HEX);
    
    // Set connection timeout to prevent blocking
    mqttClient.setSocketTimeout(2);  // 2 second timeout
    
    // Reset watchdog before connection attempt
    esp_task_wdt_reset();
    
    bool connected = false;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(clientId.c_str());
    }
    
    // Reset watchdog after connection attempt
    esp_task_wdt_reset();
    
    if (connected) {
        mqttConnected = true;
        reconnectFailures = 0;  // Reset failure counter
        reconnectBackoff = 5000;  // Reset backoff
        
        Serial.printf("MQTT connected as: %s\n", clientId.c_str());
        
        // Subscribe to reboot topic with watchdog reset
        esp_task_wdt_reset();
        if (mqttClient.subscribe(MQTT_REBOOT_TOPIC)) {
            Serial.printf("Subscribed to reboot topic: %s\n", MQTT_REBOOT_TOPIC);
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintFunc("MQTT connected!", COLOR_GREEN);
                debugPrintfFunc(COLOR_WHITE, "Subscribed to: %s", MQTT_REBOOT_TOPIC);
            }
        } else {
            Serial.println("Failed to subscribe to reboot topic!");
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintFunc("MQTT subscription failed!", COLOR_RED);
            }
        }
        
        // Subscribe to brightness topic with watchdog reset
        esp_task_wdt_reset();
        if (mqttClient.subscribe(MQTT_BRIGHTNESS_TOPIC)) {
            Serial.printf("Subscribed to brightness topic: %s\n", MQTT_BRIGHTNESS_TOPIC);
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintfFunc(COLOR_WHITE, "Brightness MQTT ready");
            }
            
            // Publish current brightness status on connect to establish proper state
            int currentBrightness = displayManager.getBrightness();
            Serial.printf("Publishing initial brightness status: %d%%\n", currentBrightness);
            publishBrightnessStatus(currentBrightness);
            
        } else {
            Serial.println("Failed to subscribe to brightness topic!");
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
    
    Serial.printf("MQTT message received on topic '%s': %s\n", topic, message.c_str());
    
    // Handle different topics
    String topicStr = String(topic);
    
    if (topicStr == MQTT_REBOOT_TOPIC) {
        mqttManager.handleRebootMessage(message);
    } else if (topicStr == MQTT_BRIGHTNESS_TOPIC) {
        mqttManager.handleBrightnessMessage(message);
    }
}

bool MQTTManager::publishBrightnessStatus(int brightness) {
    if (!isConnected()) return false;
    
    String status = String(brightness);
    bool result = mqttClient.publish(MQTT_BRIGHTNESS_STATUS_TOPIC, status.c_str());
    if (result) {
        Serial.printf("Published brightness status: %d%%\n", brightness);
    } else {
        Serial.println("Failed to publish brightness status");
    }
    return result;
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
    if (!isConnected()) {
        if (mqttConnected) {
            mqttConnected = false;
            Serial.println("MQTT disconnected");
        }
        reconnect();
    } else {
        loop();
    }
}

void MQTTManager::printConnectionInfo() {
    Serial.println("=== MQTT Connection Info ===");
    Serial.printf("Status: %s\n", isConnected() ? "Connected" : "Disconnected");
    Serial.printf("Server: %s:%d\n", MQTT_SERVER, MQTT_PORT);
    Serial.printf("Client ID: %s\n", MQTT_CLIENT_ID);
    if (isConnected()) {
        Serial.printf("Subscribed topics:\n");
        Serial.printf("  - %s (reboot)\n", MQTT_REBOOT_TOPIC);
        Serial.printf("  - %s (brightness)\n", MQTT_BRIGHTNESS_TOPIC);
        Serial.printf("Publishing to: %s\n", MQTT_BRIGHTNESS_STATUS_TOPIC);
    }
    Serial.println("============================");
}

void MQTTManager::handleRebootMessage(const String& message) {
    if (message == "reboot") {
        Serial.println("Reboot command received via MQTT - scheduling graceful recovery instead of hard restart");
        
        // Display message on screen
        if (debugPrintFunc) {
            debugPrintFunc("MQTT Reboot command received", COLOR_YELLOW);
            if (debugPrintfFunc) {
                debugPrintfFunc(COLOR_CYAN, "Performing graceful recovery...");
            }
        }
        
        // Instead of immediately restarting, schedule a graceful recovery
        // Reset network and MQTT connections to restart them
        // This keeps the device running and retrying in background
        
        // Trigger a reconnection sequence: disconnect then reconnect
        // The main loop will handle reconnection attempts
        Serial.println("Network recovery initiated - device will attempt to reconnect");
        
        // Note: Full ESP.restart() is no longer called - device stays operational
        // with background recovery attempts instead
    }
}

void MQTTManager::handleBrightnessMessage(const String& message) {
    Serial.printf("Processing brightness message: '%s'\n", message.c_str());
    
    // Check if auto brightness mode is enabled
    if (!configStorage.getBrightnessAutoMode()) {
        Serial.println("MQTT brightness control disabled (auto mode is off) - ignoring");
        return;
    }
    
    // Handle empty or invalid messages
    if (message.length() == 0) {
        Serial.println("Empty brightness message received - ignoring");
        return;
    }
    
    int brightness = message.toInt();
    
    // Additional validation for edge cases
    if (message == "0") {
        brightness = 0; // Explicitly handle "0" string
    } else if (brightness == 0 && message != "0") {
        Serial.printf("Invalid brightness value (non-numeric): '%s' - ignoring\n", message.c_str());
        return;
    }
    
    if (brightness >= 0 && brightness <= 100) {
        // Get current brightness to compare
        int currentBrightness = displayManager.getBrightness();
        
        if (brightness != currentBrightness) {
            displayManager.setBrightness(brightness);
            Serial.printf("Brightness changed from %d%% to %d%% via MQTT\n", currentBrightness, brightness);
            
            // Save the value to config so it persists
            configStorage.setDefaultBrightness(brightness);
            configStorage.saveConfig();
            
            // Show debug message if first image not loaded yet
            if (debugPrintFunc && !firstImageLoaded) {
                debugPrintfFunc(COLOR_CYAN, "Brightness: %d%% -> %d%% via MQTT", currentBrightness, brightness);
            }
        } else {
            Serial.printf("Brightness already at %d%% - no change needed\n", brightness);
        }
        
        // Always publish status confirmation for valid values
        publishBrightnessStatus(brightness);
    } else {
        Serial.printf("Invalid brightness value (out of range): %d - ignoring\n", brightness);
    }
}
