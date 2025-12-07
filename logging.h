#ifndef LOGGING_H
#define LOGGING_H

#include "config.h"  // For LogSeverity enum

// Forward declarations
class CrashLogger;
class WebConfig;
extern CrashLogger crashLogger;
extern WebConfig webConfig;

// Core logging functions - implemented in .ino
void logPrint(const char* message, LogSeverity severity = LOG_INFO);
void logPrintf(LogSeverity severity, const char* format, ...);

// Convenience macros for logging to both Serial and WebSocket with severity levels
// Use these instead of Serial.println() / Serial.printf() to enable remote console visibility

// LOG_DEBUG: Detailed diagnostic information (hidden by default in production)
#define LOG_DEBUG(msg) do { logPrint(msg, LOG_DEBUG); logPrint("\n", LOG_DEBUG); } while(0)
#define LOG_DEBUG_F(...) logPrintf(LOG_DEBUG, __VA_ARGS__)

// LOG_INFO: General informational messages (default visibility level)
#define LOG_INFO(msg) do { logPrint(msg, LOG_INFO); logPrint("\n", LOG_INFO); } while(0)
#define LOG_INFO_F(...) logPrintf(LOG_INFO, __VA_ARGS__)

// LOG_WARNING: Warning messages (potential issues, but operation continues)
#define LOG_WARNING(msg) do { logPrint(msg, LOG_WARNING); logPrint("\n", LOG_WARNING); } while(0)
#define LOG_WARNING_F(...) logPrintf(LOG_WARNING, __VA_ARGS__)

// LOG_ERROR: Error messages (operation failed, but system continues)
#define LOG_ERROR(msg) do { logPrint(msg, LOG_ERROR); logPrint("\n", LOG_ERROR); } while(0)
#define LOG_ERROR_F(...) logPrintf(LOG_ERROR, __VA_ARGS__)

// LOG_CRITICAL: Critical errors (system may be unstable)
#define LOG_CRITICAL(msg) do { logPrint(msg, LOG_CRITICAL); logPrint("\n", LOG_CRITICAL); } while(0)
#define LOG_CRITICAL_F(...) logPrintf(LOG_CRITICAL, __VA_ARGS__)

#endif // LOGGING_H
