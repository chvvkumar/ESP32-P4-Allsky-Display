#include "crash_logger.h"
#include "build_info.h"
#include <stdarg.h>
#include <esp_system.h>
#include <rom/rtc.h>

// Static RTC memory (survives reboot but not power cycle)
RTC_DATA_ATTR char CrashLogger::rtcLogBuffer[RTC_BUFFER_SIZE] = {0};
RTC_DATA_ATTR size_t CrashLogger::rtcWritePos = 0;
RTC_DATA_ATTR size_t CrashLogger::rtcLength = 0;
RTC_DATA_ATTR uint32_t CrashLogger::bootCount = 0;
RTC_DATA_ATTR uint32_t CrashLogger::crashMarker = 0;

// Global instance
CrashLogger crashLogger;

CrashLogger::CrashLogger() : 
    ramLogBuffer(nullptr),
    ramWritePos(0),
    ramLength(0),
    initialized(false),
    sessionStartTime(0) {
}

CrashLogger::~CrashLogger() {
    if (ramLogBuffer) {
        free(ramLogBuffer);
    }
}

void CrashLogger::begin() {
    // Allocate RAM buffer
    ramLogBuffer = (char*)malloc(RAM_BUFFER_SIZE);
    if (!ramLogBuffer) {
        Serial.println("[CrashLogger] ✗ Failed to allocate RAM buffer");
        return;
    }
    memset(ramLogBuffer, 0, RAM_BUFFER_SIZE);
    
    // Increment boot count
    bootCount++;
    sessionStartTime = millis();
    
    // Open NVS
    prefs.begin("crash_log", false);
    
    initialized = true;
    
    // Check reset reason to detect crashes
    esp_reset_reason_t reset_reason = esp_reset_reason();
    bool wasCrash = false;
    
    switch (reset_reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            wasCrash = true;
            crashMarker = 0xDEADBEEF;  // Mark as crash
            break;
        case ESP_RST_POWERON:
        case ESP_RST_SW:
        case ESP_RST_SDIO:
        case ESP_RST_BROWNOUT:
        case ESP_RST_DEEPSLEEP:
        case ESP_RST_USB:
        case ESP_RST_JTAG:
        case ESP_RST_UNKNOWN:
        default:
            break;
    }
    
    // Log boot event with timestamp
    char bootMsg[256];
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) && timeinfo.tm_year > (2016 - 1900)) {
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        snprintf(bootMsg, sizeof(bootMsg), "\n===== BOOT #%lu at %lu ms [%s] =====\n", 
                 bootCount, sessionStartTime, timestamp);
    } else {
        snprintf(bootMsg, sizeof(bootMsg), "\n===== BOOT #%lu at %lu ms =====\n", 
                 bootCount, sessionStartTime);
    }
    log(bootMsg);
    
    // Check for crash
    if (crashMarker != 0 || wasCrash) {
        const char* resetReasonStr = "UNKNOWN";
        switch (reset_reason) {
            case ESP_RST_POWERON: resetReasonStr = "POWERON"; break;
            case ESP_RST_SW: resetReasonStr = "SOFTWARE"; break;
            case ESP_RST_PANIC: resetReasonStr = "PANIC/EXCEPTION"; break;
            case ESP_RST_INT_WDT: resetReasonStr = "INTERRUPT_WATCHDOG"; break;
            case ESP_RST_TASK_WDT: resetReasonStr = "TASK_WATCHDOG"; break;
            case ESP_RST_WDT: resetReasonStr = "WATCHDOG"; break;
            case ESP_RST_DEEPSLEEP: resetReasonStr = "DEEPSLEEP"; break;
            case ESP_RST_BROWNOUT: resetReasonStr = "BROWNOUT"; break;
            case ESP_RST_SDIO: resetReasonStr = "SDIO"; break;
            case ESP_RST_USB: resetReasonStr = "USB"; break;
            case ESP_RST_JTAG: resetReasonStr = "JTAG"; break;
            default: resetReasonStr = "UNKNOWN"; break;
        }
        
        char crashMsg[256];
        snprintf(crashMsg, sizeof(crashMsg), 
                 "[CrashLogger] !!! CRASH DETECTED !!! Reset reason: %s\n", resetReasonStr);
        log(crashMsg);
        
        // Save crash logs to NVS immediately
        saveToNVS();
        
        // Clear crash marker
        crashMarker = 0;
        
        Serial.printf("[CrashLogger] Crash detected - Reset reason: %s - Logs preserved\n", resetReasonStr);
    } else {
        log("[CrashLogger] Normal boot\n");
        // Log firmware version on normal boot
        char versionMsg[128];
        snprintf(versionMsg, sizeof(versionMsg), "Firmware: %s (%s) - Built: %s %s\n", 
                 GIT_BRANCH, GIT_COMMIT_HASH, BUILD_DATE, BUILD_TIME);
        log(versionMsg);
    }
    
    // Report buffer status
    Serial.printf("[CrashLogger] ✓ Initialized - Boot #%lu, RTC: %d/%d bytes, RAM: %d/%d bytes\n",
                  bootCount, rtcLength, RTC_BUFFER_SIZE, ramLength, RAM_BUFFER_SIZE);
}

void CrashLogger::writeToRingBuffer(char* buffer, size_t& writePos, size_t& length, 
                                     size_t bufferSize, const char* msg, size_t msgLen) {
    if (msgLen == 0) return;
    
    // If message is larger than buffer, only take the end
    if (msgLen >= bufferSize) {
        msg += (msgLen - bufferSize + 1);
        msgLen = bufferSize - 1;
    }
    
    // Write to ring buffer
    for (size_t i = 0; i < msgLen; i++) {
        buffer[writePos] = msg[i];
        writePos = (writePos + 1) % bufferSize;
        
        // Track actual content length (up to buffer size)
        if (length < bufferSize) {
            length++;
        }
    }
}

size_t CrashLogger::readFromRingBuffer(char* buffer, size_t writePos, size_t length, 
                                        size_t bufferSize, char* outBuffer, size_t outSize) {
    if (length == 0 || outSize == 0) return 0;
    
    // Calculate read start position
    size_t readPos;
    size_t readLen = (length < outSize - 1) ? length : (outSize - 1);
    
    if (length >= bufferSize) {
        // Buffer is full, start from write position (oldest data)
        readPos = writePos;
    } else {
        // Buffer not full, start from beginning
        readPos = 0;
    }
    
    // Read from ring buffer
    for (size_t i = 0; i < readLen; i++) {
        outBuffer[i] = buffer[readPos];
        readPos = (readPos + 1) % bufferSize;
    }
    
    outBuffer[readLen] = '\0';
    return readLen;
}

void CrashLogger::log(const char* message) {
    if (!initialized && !ramLogBuffer) return;
    
    size_t msgLen = strlen(message);
    if (msgLen == 0) return;
    
    // Write to RTC buffer (survives reboot)
    writeToRingBuffer(rtcLogBuffer, rtcWritePos, rtcLength, RTC_BUFFER_SIZE, message, msgLen);
    
    // Write to RAM buffer (current session)
    writeToRingBuffer(ramLogBuffer, ramWritePos, ramLength, RAM_BUFFER_SIZE, message, msgLen);
}

void CrashLogger::logf(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    log(buffer);
}

void CrashLogger::markCrash() {
    crashMarker = 0xDEADBEEF;  // Magic number indicating crash
    
    // Log crash marker
    char crashMsg[128];
    snprintf(crashMsg, sizeof(crashMsg), "[CrashLogger] !!! CRASH DETECTED at %lu ms !!!\n", 
             millis());
    log(crashMsg);
    
    // Save to NVS immediately (crash detected - must preserve logs)
    saveToNVS();
}

void CrashLogger::saveBeforeReboot() {
    // Called before intentional reboot to preserve current state
    log("[CrashLogger] Saving logs before intentional reboot\n");
    saveToNVS();
}

bool CrashLogger::wasLastBootCrash() {
    return (crashMarker == 0xDEADBEEF);
}

String CrashLogger::getRAMLogs() {
    if (!ramLogBuffer || ramLength == 0) {
        return String("[CrashLogger] No RAM logs available\n");
    }
    
    char* buffer = (char*)malloc(RAM_BUFFER_SIZE);
    if (!buffer) return String("[CrashLogger] Memory allocation failed\n");
    
    size_t len = readFromRingBuffer(ramLogBuffer, ramWritePos, ramLength, 
                                     RAM_BUFFER_SIZE, buffer, RAM_BUFFER_SIZE);
    
    String result(buffer);
    free(buffer);
    
    return result;
}

String CrashLogger::getRTCLogs() {
    if (rtcLength == 0) {
        return String("[CrashLogger] No RTC logs available\n");
    }
    
    char* buffer = (char*)malloc(RTC_BUFFER_SIZE);
    if (!buffer) return String("[CrashLogger] Memory allocation failed\n");
    
    size_t len = readFromRingBuffer(rtcLogBuffer, rtcWritePos, rtcLength, 
                                     RTC_BUFFER_SIZE, buffer, RTC_BUFFER_SIZE);
    
    String result(buffer);
    free(buffer);
    
    return result;
}

String CrashLogger::getNVSLogs() {
    if (!initialized) return String("[CrashLogger] Not initialized\n");
    
    // Check if NVS has logs
    if (!prefs.isKey("log_data")) {
        return String("[CrashLogger] No NVS logs available\n");
    }
    
    size_t logSize = prefs.getBytesLength("log_data");
    if (logSize == 0) {
        return String("[CrashLogger] NVS logs empty\n");
    }
    
    char* buffer = (char*)malloc(logSize + 1);
    if (!buffer) return String("[CrashLogger] Memory allocation failed\n");
    
    size_t readLen = prefs.getBytes("log_data", buffer, logSize);
    buffer[readLen] = '\0';
    
    String result = String("[CrashLogger] NVS logs from boot #") + 
                    String(prefs.getUInt("log_boot", 0)) + ":\n" + String(buffer);
    free(buffer);
    
    return result;
}

String CrashLogger::getRecentLogs(size_t maxBytes) {
    String result;
    result.reserve(maxBytes);
    
    // Start with header
    result += "===== CRASH LOGGER DUMP =====\n";
    result += "Boot Count: " + String(bootCount) + "\n";
    result += "Session Uptime: " + String(millis() - sessionStartTime) + " ms\n";
    result += "Last Boot Crash: " + String(crashMarker == 0xDEADBEEF ? "YES" : "NO") + "\n";
    
    // Show logs in chronological order: oldest (NVS) → middle (RTC) → newest (RAM)
    // Check if we have NVS logs from previous boot (oldest)
    if (prefs.isKey("log_data")) {
        result += "\n--- NVS Logs (Preserved from Previous Boot) ---\n";
        result += getNVSLogs();
    }
    
    result += "\n--- RTC Logs (Since Last Reboot) ---\n";
    result += getRTCLogs();
    
    result += "\n--- RAM Logs (Current Session) ---\n";
    result += getRAMLogs();
    
    result += "\n===== END CRASH LOGGER DUMP =====\n";
    
    return result;
}

void CrashLogger::saveToNVS() {
    if (!initialized) return;
    
    Serial.println("[CrashLogger] Saving logs to NVS...");
    
    // Save RTC buffer to NVS
    if (rtcLength > 0) {
        char* buffer = (char*)malloc(RTC_BUFFER_SIZE);
        if (buffer) {
            size_t len = readFromRingBuffer(rtcLogBuffer, rtcWritePos, rtcLength, 
                                           RTC_BUFFER_SIZE, buffer, RTC_BUFFER_SIZE);
            
            // Truncate if needed
            if (len > NVS_MAX_SIZE) {
                buffer += (len - NVS_MAX_SIZE);
                len = NVS_MAX_SIZE;
            }
            
            prefs.putBytes("log_data", buffer, len);
            prefs.putUInt("log_boot", bootCount);
            prefs.putULong("log_time", millis());
            
            free(buffer);
            Serial.printf("[CrashLogger] ✓ Saved %d bytes to NVS\n", len);
        }
    }
}

void CrashLogger::clearAll() {
    // Clear RAM
    if (ramLogBuffer) {
        memset(ramLogBuffer, 0, RAM_BUFFER_SIZE);
    }
    ramWritePos = 0;
    ramLength = 0;
    
    // Clear RTC (note: this will be lost on next reboot anyway)
    memset(rtcLogBuffer, 0, RTC_BUFFER_SIZE);
    rtcWritePos = 0;
    rtcLength = 0;
    
    // Clear NVS
    if (initialized) {
        prefs.remove("log_data");
        prefs.remove("log_boot");
        prefs.remove("log_time");
    }
    
    // Clear crash marker
    crashMarker = 0;
    
    Serial.println("[CrashLogger] ✓ All logs cleared");
}
