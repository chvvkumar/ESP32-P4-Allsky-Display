/**
 * OTA Manager Implementation
 * Handles Over-The-Air firmware updates with progress tracking
 */

#include "ota_manager.h"
#include "display_manager.h"
#include <Update.h>

// Global instance
OTAManager otaManager;

OTAManager::OTAManager() 
    : status(OTA_UPDATE_IDLE), progress(0) {
    statusMessage[0] = '\0';
}

void OTAManager::begin() {
    status = OTA_UPDATE_IDLE;
    progress = 0;
    statusMessage[0] = '\0';
    
    if (debugPrint) {
        debugPrint("OTA Manager initialized", COLOR_GREEN);
    }
}

void OTAManager::setDebugFunction(
    std::function<void(const char*, uint16_t)> debugPrintFunc
) {
    debugPrint = debugPrintFunc;
}

void OTAManager::setStatus(OTAUpdateStatus newStatus, const char* message) {
    status = newStatus;
    
    if (message) {
        strncpy(statusMessage, message, sizeof(statusMessage) - 1);
        statusMessage[sizeof(statusMessage) - 1] = '\0';
        
        if (debugPrint) {
            debugPrint(message, COLOR_CYAN);
        }
    }
}

void OTAManager::setProgress(uint8_t percent) {
    progress = (percent > 100) ? 100 : percent;
}

void OTAManager::reset() {
    status = OTA_UPDATE_IDLE;
    progress = 0;
    statusMessage[0] = '\0';
}

void OTAManager::displayProgress(const char* operation, uint8_t percent) {
    progress = (percent > 100) ? 100 : percent;
    
    // Update status message and display
    snprintf(statusMessage, sizeof(statusMessage), "%s: %d%%", operation, progress);
    
    if (debugPrint) {
        debugPrint(statusMessage, COLOR_CYAN);
    }
}
