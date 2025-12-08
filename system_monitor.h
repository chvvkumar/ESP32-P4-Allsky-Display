#pragma once
#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>
#include "config.h"

extern "C" {
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

class SystemMonitor {
private:
    unsigned long lastWatchdogReset;
    unsigned long lastMemoryCheck;
    unsigned long lastSerialFlush;
    size_t minFreeHeap;
    size_t minFreePsram;
    bool systemHealthy;

public:
    SystemMonitor();
    
    // Initialization
    bool begin(unsigned long timeoutMs = WATCHDOG_TIMEOUT_MS);
    
    // Watchdog management
    void resetWatchdog();
    void forceResetWatchdog();
    
    // System health monitoring
    void checkSystemHealth();
    bool isSystemHealthy() const;
    
    // Memory monitoring
    size_t getMinFreeHeap() const;
    size_t getMinFreePsram() const;
    size_t getCurrentFreeHeap() const;
    size_t getCurrentFreePsram() const;
    
    // Serial management
    void flushSerial();
    
    // Safe operations
    void safeYield();
    void safeDelay(unsigned long ms);
    
    // Update function - call this in main loop
    void update();
    
    // Print memory status
    void printMemoryStatus();
};

// Global instance
extern SystemMonitor systemMonitor;

#endif // SYSTEM_MONITOR_H
