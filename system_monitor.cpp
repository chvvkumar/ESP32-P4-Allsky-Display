#include "system_monitor.h"
#include "logging.h"

// Global instance
SystemMonitor systemMonitor;

SystemMonitor::SystemMonitor() :
    lastWatchdogReset(0),
    lastMemoryCheck(0),
    lastSerialFlush(0),
    lastStackCheck(0),
    minFreeHeap(SIZE_MAX),
    minFreePsram(SIZE_MAX),
    systemHealthy(true)
{
}

bool SystemMonitor::begin(unsigned long timeoutMs) {
    LOG_DEBUG("[SystemMonitor] Initializing system monitor and watchdog");
    // Try to initialize watchdog timer for system stability
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = timeoutMs,
        .idle_core_mask = WATCHDOG_IDLE_CORE_MASK,
        .trigger_panic = WATCHDOG_TRIGGER_PANIC
    };
    
    LOG_DEBUG_F("[SystemMonitor] Watchdog timeout: %lu ms\n", timeoutMs);
    esp_err_t result = esp_task_wdt_init(&wdt_config);
    if (result == ESP_ERR_INVALID_STATE) {
        LOG_DEBUG("[SystemMonitor] Watchdog already initialized");
    } else if (result != ESP_OK) {
        LOG_ERROR_F("[SystemMonitor] Watchdog init failed: %s\n", esp_err_to_name(result));
        return false;
    }
    
    // Add current task to watchdog
    result = esp_task_wdt_add(NULL);
    if (result == ESP_ERR_INVALID_ARG) {
        LOG_DEBUG("[SystemMonitor] Task already added to watchdog");
    } else if (result != ESP_OK) {
        LOG_ERROR_F("[SystemMonitor] Watchdog add task failed: %s\n", esp_err_to_name(result));
        return false;
    }
    
    // Initialize memory tracking
    minFreeHeap = ESP.getFreeHeap();
    minFreePsram = ESP.getFreePsram();
    LOG_DEBUG_F("[SystemMonitor] Initial heap: %d bytes, PSRAM: %d bytes\n", minFreeHeap, minFreePsram);
    LOG_DEBUG("[SystemMonitor] System monitor initialization complete");
    
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
            LOG_CRITICAL_F("[SystemMonitor] Low memory detected - Heap: %d bytes (threshold: %d), PSRAM: %d bytes (threshold: %d)\n", 
                          freeHeap, CRITICAL_HEAP_THRESHOLD, freePsram, CRITICAL_PSRAM_THRESHOLD);
            
            // Force garbage collection
            if (heapCritical) {
                LOG_WARNING("[SystemMonitor] Attempting heap cleanup");
                // Could add specific cleanup here if needed
            }
        } else {
            if (!systemHealthy) {
                LOG_INFO("[SystemMonitor] System health recovered");
            }
            systemHealthy = true;
        }
        
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

void SystemMonitor::checkStackHighWaterMarks() {
    unsigned long now = millis();
    // Check every 60 seconds to avoid log spam
    if (now - lastStackCheck < 60000) {
        return;
    }
    lastStackCheck = now;

    // Log the current task's stack high water mark
    UBaseType_t hwm = uxTaskGetStackHighWaterMark(NULL);
    LOG_INFO_F("[SystemMonitor] Current task stack HWM: %u words (%u bytes free)\n",
               hwm, hwm * sizeof(StackType_t));

    if (hwm < 256) {
        LOG_WARNING_F("[SystemMonitor] Current task stack is low! HWM: %u words\n", hwm);
    }

    // Enumerate all known named tasks and log their stack usage
    const char* taskNames[] = {"ImageDownloader", "HARestClient", NULL};
    for (int i = 0; taskNames[i] != NULL; i++) {
        TaskHandle_t handle = xTaskGetHandle(taskNames[i]);
        if (handle != NULL) {
            UBaseType_t taskHwm = uxTaskGetStackHighWaterMark(handle);
            LOG_INFO_F("[SystemMonitor] Task '%s' stack HWM: %u words (%u bytes free)\n",
                       taskNames[i], taskHwm, taskHwm * sizeof(StackType_t));
            if (taskHwm < 256) {
                LOG_WARNING_F("[SystemMonitor] Task '%s' stack is low! HWM: %u words\n",
                             taskNames[i], taskHwm);
            }
        }
    }
}

void SystemMonitor::update() {
    forceResetWatchdog();
    checkSystemHealth();
    checkStackHighWaterMarks();
    flushSerial();
}

void SystemMonitor::printMemoryStatus() {
    size_t freeHeap = getCurrentFreeHeap();
    size_t freePsram = getCurrentFreePsram();
    
    LOG_INFO("[SystemMonitor] === Memory Status ===");
    LOG_INFO_F("[SystemMonitor] Current - Heap: %d bytes, PSRAM: %d bytes\n", freeHeap, freePsram);
    LOG_INFO_F("[SystemMonitor] Minimum - Heap: %d bytes, PSRAM: %d bytes\n", minFreeHeap, minFreePsram);
    LOG_INFO_F("[SystemMonitor] System Health: %s\n", systemHealthy ? "HEALTHY" : "CRITICAL");
    LOG_INFO("[SystemMonitor] =======================");
}
