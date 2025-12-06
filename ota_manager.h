/**
 * OTA Manager - Handles Over-The-Air firmware updates
 * Provides progress tracking and display integration for OTA operations
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <functional>

// OTA update status
enum OTAUpdateStatus {
    OTA_UPDATE_IDLE,
    OTA_UPDATE_IN_PROGRESS,
    OTA_UPDATE_SUCCESS,
    OTA_UPDATE_FAILED
};

class OTAManager {
public:
    OTAManager();
    
    // Initialize OTA manager with debug functions
    void begin();
    void setDebugFunction(
        std::function<void(const char*, uint16_t)> debugPrintFunc
    );
    
    // Status and progress
    OTAUpdateStatus getStatus() const { return status; }
    uint8_t getProgress() const { return progress; }
    const char* getStatusMessage() const { return statusMessage; }
    
    // Manual status update for external OTA handlers (ArduinoOTA, Web OTA)
    void setStatus(OTAUpdateStatus newStatus, const char* message = nullptr);
    void setProgress(uint8_t percent);
    void reset();
    
    // Display progress on screen
    void displayProgress(const char* operation, uint8_t percent);
    
private:
    OTAUpdateStatus status;
    uint8_t progress;
    char statusMessage[128];
    
    // Debug function pointer
    std::function<void(const char*, uint16_t)> debugPrint;
};

extern OTAManager otaManager;

#endif // OTA_MANAGER_H
