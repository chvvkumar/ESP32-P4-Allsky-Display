#include "ha_rest_client.h"
#include "config_storage.h"
#include "display_manager.h"
#include "network_manager.h"
#include "logging.h"
#include "watchdog_scope.h"
#include <WiFi.h>

// Global instance
HARestClient haRestClient;

// FreeRTOS task configuration
#define HA_REST_TASK_STACK_SIZE 8192
#define HA_REST_TASK_PRIORITY 1
#define HA_REST_TASK_CORE 0  // Network Core

void HARestClient::begin() {
    if (_taskRunning) {
        LOG_WARNING("[HARestClient] Task already running, skipping begin()");
        return;
    }
    
    if (!configStorage.getUseHARestControl()) {
        LOG_INFO("[HARestClient] HA REST Control disabled in config, not starting task");
        return;
    }
    
    LOG_INFO("[HARestClient] Starting HA REST client task on Core 0");
    
    // Create FreeRTOS task pinned to Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskLoop,                    // Task function
        "HARestClient",              // Task name
        HA_REST_TASK_STACK_SIZE,     // Stack size (bytes)
        this,                        // Task parameter (instance pointer)
        HA_REST_TASK_PRIORITY,       // Priority
        &_taskHandle,                // Task handle
        HA_REST_TASK_CORE            // Core ID (0 = Network Core)
    );
    
    if (result == pdPASS) {
        _taskRunning = true;
        LOG_INFO("[HARestClient] Task created successfully");
    } else {
        LOG_ERROR("[HARestClient] Failed to create task");
    }
}

void HARestClient::stop() {
    if (!_taskRunning) {
        return;
    }
    
    LOG_INFO("[HARestClient] Stopping task");
    _taskRunning = false;
    
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
}

void HARestClient::taskLoop(void* parameter) {
    HARestClient* instance = static_cast<HARestClient*>(parameter);
    
    LOG_INFO("[HARestClient] Task loop started");
    
    // Initial delay before first check
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    while (instance->_taskRunning) {
        // Only perform check if enabled and WiFi is connected
        if (configStorage.getUseHARestControl() && wifiManager.isConnected()) {
            instance->performCheck();
        }
        
        // Wait for configured poll interval
        unsigned long pollInterval = configStorage.getHAPollInterval() * 1000UL;  // Convert to milliseconds
        vTaskDelay(pdMS_TO_TICKS(pollInterval));
    }
    
    LOG_INFO("[HARestClient] Task loop ended");
    vTaskDelete(nullptr);
}

void HARestClient::performCheck() {
    WATCHDOG_SCOPE();  // Reset watchdog during HTTP operation
    
    String baseUrl = configStorage.getHABaseUrl();
    String token = configStorage.getHAAccessToken();
    String entityId = configStorage.getHALightSensorEntity();
    
    // Validate configuration
    if (baseUrl.isEmpty() || token.isEmpty() || entityId.isEmpty()) {
        LOG_WARNING("[HARestClient] Configuration incomplete, skipping check");
        return;
    }
    
    // Build API URL
    String url = baseUrl;
    if (!url.endsWith("/")) {
        url += "/";
    }
    url += "api/states/" + entityId;
    
    LOG_DEBUG_F("[HARestClient] Querying: %s\n", url.c_str());
    
    HTTPClient http;
    http.begin(url);
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);  // 10 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        // Parse JSON response
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            const char* stateStr = doc["state"];
            
            if (stateStr != nullptr && strcmp(stateStr, "unavailable") != 0 && strcmp(stateStr, "unknown") != 0) {
                float lux = atof(stateStr);
                _lastSensorValue = lux;
                
                int brightness = calculateBrightness(lux);
                _lastBrightness = brightness;
                
                LOG_INFO_F("[HARestClient] Sensor: %.1f lux -> Brightness: %d%%\n", lux, brightness);
                
                // Apply brightness to display
                displayManager.setBrightness(brightness);
            } else {
                LOG_WARNING_F("[HARestClient] Sensor unavailable (state: %s)\n", stateStr);
            }
        } else {
            LOG_ERROR_F("[HARestClient] JSON parse error: %s\n", error.c_str());
        }
    } else {
        LOG_ERROR_F("[HARestClient] HTTP GET failed: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
    }
    
    http.end();
}

int HARestClient::calculateBrightness(float lux) {
    int mappingMode = configStorage.getLightSensorMappingMode();
    float minLux = configStorage.getLightSensorMinLux();
    float maxLux = configStorage.getLightSensorMaxLux();
    int minBrightness = configStorage.getDisplayMinBrightness();
    int maxBrightness = configStorage.getDisplayMaxBrightness();
    
    // Clamp input lux to configured range
    if (lux < minLux) lux = minLux;
    if (lux > maxLux) lux = maxLux;
    
    int brightness = minBrightness;
    
    switch (mappingMode) {
        case 0: {  // Linear mapping
            float luxRange = maxLux - minLux;
            if (luxRange > 0) {
                float ratio = (lux - minLux) / luxRange;
                brightness = minBrightness + (int)(ratio * (maxBrightness - minBrightness));
            }
            break;
        }
        
        case 1: {  // Logarithmic mapping (human eye perception)
            // Use log10 for better perception curve
            // Add 1 to avoid log(0)
            float logMin = log10(minLux + 1);
            float logMax = log10(maxLux + 1);
            float logCurrent = log10(lux + 1);
            
            float logRange = logMax - logMin;
            if (logRange > 0) {
                float ratio = (logCurrent - logMin) / logRange;
                brightness = minBrightness + (int)(ratio * (maxBrightness - minBrightness));
            }
            break;
        }
        
        case 2: {  // Threshold switch (day/night)
            float midPoint = (minLux + maxLux) / 2.0f;
            brightness = (lux >= midPoint) ? maxBrightness : minBrightness;
            break;
        }
        
        default:
            LOG_ERROR_F("[HARestClient] Unknown mapping mode: %d, using linear\n", mappingMode);
            // Fall through to linear as default
            float luxRange = maxLux - minLux;
            if (luxRange > 0) {
                float ratio = (lux - minLux) / luxRange;
                brightness = minBrightness + (int)(ratio * (maxBrightness - minBrightness));
            }
            break;
    }
    
    // Clamp output brightness to valid range
    if (brightness < 0) brightness = 0;
    if (brightness > 100) brightness = 100;
    
    return brightness;
}

float HARestClient::getLastSensorValue() {
    return _lastSensorValue;
}

int HARestClient::getLastBrightness() {
    return _lastBrightness;
}

bool HARestClient::isRunning() {
    return _taskRunning;
}
