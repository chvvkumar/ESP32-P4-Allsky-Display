#pragma once
#ifndef HA_REST_CLIENT_H
#define HA_REST_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * Home Assistant REST API Client for Brightness Control
 * 
 * Polls a Home Assistant light sensor via REST API and automatically
 * adjusts screen brightness based on the sensor reading. Runs on Core 0
 * (Network Core) in a dedicated FreeRTOS task to prevent UI blocking.
 * 
 * Features:
 * - Three mapping modes: Linear, Logarithmic, Threshold
 * - Configurable poll interval
 * - Configurable Lux range and brightness range
 * - Non-blocking operation on dedicated core
 */
class HARestClient {
public:
    /**
     * Initialize and start the REST client task on Core 0
     * Must be called once during setup()
     */
    void begin();
    
    /**
     * Stop the REST client task (if running)
     */
    void stop();
    
    /**
     * Get the last sensor value retrieved
     * @return Lux value, or -1 if no reading available
     */
    float getLastSensorValue();
    
    /**
     * Get the last calculated brightness
     * @return Brightness (0-100), or -1 if no calculation yet
     */
    int getLastBrightness();
    
    /**
     * Check if the REST client task is running
     * @return true if task is active
     */
    bool isRunning();
    
private:
    /**
     * FreeRTOS task entry point (static)
     * Runs the main polling loop on Core 0
     */
    static void taskLoop(void* parameter);
    
    /**
     * Perform a single REST API check
     * Called periodically by taskLoop
     */
    void performCheck();
    
    /**
     * Calculate brightness from Lux value using configured mapping mode
     * @param lux Current light sensor reading
     * @return Calculated brightness (0-100)
     */
    int calculateBrightness(float lux);
    
    // Task handle for the Core 0 FreeRTOS task
    TaskHandle_t _taskHandle = nullptr;
    
    // Last retrieved values
    float _lastSensorValue = -1.0f;
    int _lastBrightness = -1;
    
    // Task control
    volatile bool _taskRunning = false;
};

// Global instance
extern HARestClient haRestClient;

#endif // HA_REST_CLIENT_H
