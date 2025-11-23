#include "system_monitor.h"

// Global instance
SystemMonitor systemMonitor;

SystemMonitor::SystemMonitor() :
    lastWatchdogReset(0),
    lastMemoryCheck(0),
    lastSerialFlush(0),
    minFreeHeap(SIZE_MAX),
    minFreePsram(SIZE_MAX),
    systemHealthy(true)
{
}

bool SystemMonitor::begin() {
    Serial.println("Configuring watchdog timer...");
    
    // Try to initialize watchdog timer for system stability
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = WATCHDOG_IDLE_CORE_MASK,
        .trigger_panic = WATCHDOG_TRIGGER_PANIC
    };
    
    esp_err_t result = esp_task_wdt_init(&wdt_config);
    if (result == ESP_ERR_INVALID_STATE) {
        // Watchdog already initialized, that's fine
        Serial.println("Watchdog already initialized, using existing configuration");
    } else if (result != ESP_OK) {
        Serial.printf("Watchdog init failed: %s\n", esp_err_to_name(result));
        return false;
    } else {
        Serial.println("Watchdog timer initialized");
    }
    
    // Add current task to watchdog
    result = esp_task_wdt_add(NULL);
    if (result == ESP_ERR_INVALID_ARG) {
        // Task already added, that's fine
        Serial.println("Task already added to watchdog");
    } else if (result != ESP_OK) {
        Serial.printf("Watchdog add task failed: %s\n", esp_err_to_name(result));
        return false;
    } else {
        Serial.println("Task added to watchdog");
    }
    
    Serial.println("Watchdog timer configured successfully");
    
    // Initialize memory tracking
    minFreeHeap = ESP.getFreeHeap();
    minFreePsram = ESP.getFreePsram();
    Serial.printf("Initial memory - Heap: %d, PSRAM: %d\n", minFreeHeap, minFreePsram);
    
    return true;
}

void SystemMonitor::resetWatchdog() {
    unsigned long now = millis();
    if (now - lastWatchdogReset >= WATCHDOG_RESET_INTERVAL) {
        esp_task_wdt_reset();
        lastWatchdogReset = now;
    }
}

void SystemMonitor::forceResetWatchdog() {
    esp_task_wdt_reset();
    lastWatchdogReset = millis();
}

void SystemMonitor::checkSystemHealth() {
    unsigned long now = millis();
    if (now - lastMemoryCheck >= MEMORY_CHECK_INTERVAL) {
        size_t freeHeap = ESP.getFreeHeap();
        size_t freePsram = ESP.getFreePsram();
        
        // Track minimum memory levels
        if (freeHeap < minFreeHeap) minFreeHeap = freeHeap;
        if (freePsram < minFreePsram) minFreePsram = freePsram;
        
        // Check for critical memory levels
        bool heapCritical = freeHeap < CRITICAL_HEAP_THRESHOLD;
        bool psramCritical = freePsram < CRITICAL_PSRAM_THRESHOLD;
        
        if (heapCritical || psramCritical) {
            systemHealthy = false;
            Serial.printf("CRITICAL: Low memory - Heap: %d, PSRAM: %d\n", freeHeap, freePsram);
            
            // Force garbage collection
            if (heapCritical) {
                Serial.println("Attempting heap cleanup...");
                // Could add specific cleanup here if needed
            }
        } else {
            systemHealthy = true;
        }
        
        // Log memory status periodically
        Serial.printf("Memory status - Heap: %d (min: %d), PSRAM: %d (min: %d)\n", 
                     freeHeap, minFreeHeap, freePsram, minFreePsram);
        
        lastMemoryCheck = now;
    }
}

bool SystemMonitor::isSystemHealthy() const {
    return systemHealthy;
}

size_t SystemMonitor::getMinFreeHeap() const {
    return minFreeHeap;
}

size_t SystemMonitor::getMinFreePsram() const {
    return minFreePsram;
}

size_t SystemMonitor::getCurrentFreeHeap() const {
    return ESP.getFreeHeap();
}

size_t SystemMonitor::getCurrentFreePsram() const {
    return ESP.getFreePsram();
}

void SystemMonitor::flushSerial() {
    unsigned long now = millis();
    if (now - lastSerialFlush >= SERIAL_FLUSH_INTERVAL) {
        Serial.flush();
        lastSerialFlush = now;
    }
}

void SystemMonitor::safeYield() {
    resetWatchdog();
    yield();
    vTaskDelay(1); // Give other tasks a chance to run
}

void SystemMonitor::safeDelay(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        resetWatchdog();
        delay(min(100UL, ms - (millis() - start)));
    }
}

void SystemMonitor::update() {
    forceResetWatchdog();
    checkSystemHealth();
    flushSerial();
}

void SystemMonitor::printMemoryStatus() {
    size_t freeHeap = getCurrentFreeHeap();
    size_t freePsram = getCurrentFreePsram();
    
    Serial.printf("Current Memory - Heap: %d bytes, PSRAM: %d bytes\n", freeHeap, freePsram);
    Serial.printf("Minimum Memory - Heap: %d bytes, PSRAM: %d bytes\n", minFreeHeap, minFreePsram);
    Serial.printf("System Health: %s\n", systemHealthy ? "HEALTHY" : "CRITICAL");
}
