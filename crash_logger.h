#ifndef CRASH_LOGGER_H
#define CRASH_LOGGER_H

#include <Arduino.h>
#include <Preferences.h>

// Ring buffer for crash logs and boot messages
class CrashLogger {
private:
    static const size_t RTC_BUFFER_SIZE = 4096;  // 4KB in RTC memory (survives reboot)
    static const size_t RAM_BUFFER_SIZE = 8192;  // 8KB in RAM for current session
    static const size_t NVS_MAX_SIZE = 4000;     // ~4KB max per NVS blob
    
    // RTC memory buffer (survives soft reboot but not power loss)
    static RTC_DATA_ATTR char rtcLogBuffer[RTC_BUFFER_SIZE];
    static RTC_DATA_ATTR size_t rtcWritePos;
    static RTC_DATA_ATTR size_t rtcLength;
    static RTC_DATA_ATTR uint32_t bootCount;
    static RTC_DATA_ATTR uint32_t crashMarker;  // Non-zero if last boot was a crash
    
    // RAM buffer for current session (fastest access)
    char* ramLogBuffer;
    size_t ramWritePos;
    size_t ramLength;
    
    Preferences prefs;
    bool initialized;
    uint32_t sessionStartTime;
    
    // Write to ring buffer
    void writeToRingBuffer(char* buffer, size_t& writePos, size_t& length, size_t bufferSize, const char* msg, size_t msgLen);
    
    // Read from ring buffer
    size_t readFromRingBuffer(char* buffer, size_t writePos, size_t length, size_t bufferSize, char* outBuffer, size_t outSize);

public:
    CrashLogger();
    ~CrashLogger();
    
    // Initialize logger and check for crash
    void begin();
    
    // Log a message to all buffers
    void log(const char* message);
    void logf(const char* format, ...);
    
    // Mark that we're about to crash (call in panic handler)
    void markCrash();
    
    // Save logs before intentional reboot
    void saveBeforeReboot();
    
    // Check if last boot was a crash
    bool wasLastBootCrash();
    
    // Get boot count
    uint32_t getBootCount() { return bootCount; }
    
    // Retrieve logs (from newest to oldest)
    String getRecentLogs(size_t maxBytes = 4096);
    String getRTCLogs();
    String getRAMLogs();
    String getNVSLogs();
    
    // Save current logs to NVS (call periodically or before intentional reboot)
    void saveToNVS();
    
    // Clear all logs
    void clearAll();
    
    // Get stats
    size_t getRTCUsage() { return rtcLength; }
    size_t getRAMUsage() { return ramLength; }
};

// Global instance
extern CrashLogger crashLogger;

#endif // CRASH_LOGGER_H
