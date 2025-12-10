#ifndef DEVICE_HEALTH_H
#define DEVICE_HEALTH_H

#include <Arduino.h>
#include "config.h"

// Health status levels
enum HealthStatus {
    HEALTH_EXCELLENT = 0,  // All systems optimal
    HEALTH_GOOD = 1,       // Minor issues, fully functional
    HEALTH_WARNING = 2,    // Issues detected, may need attention
    HEALTH_CRITICAL = 3,   // Critical issues, action required
    HEALTH_FAILING = 4     // System failing or unstable
};

// Component-specific health metrics
struct MemoryHealth {
    size_t freeHeap;
    size_t totalHeap;
    size_t minFreeHeap;
    size_t freePsram;
    size_t totalPsram;
    size_t minFreePsram;
    float heapUsagePercent;
    float psramUsagePercent;
    HealthStatus status;
    String message;
};

struct NetworkHealth {
    bool connected;
    int rssi;
    unsigned long uptime;
    unsigned long disconnectCount;
    HealthStatus status;
    String message;
};

struct MQTTHealth {
    bool connected;
    unsigned long reconnectCount;
    unsigned long lastSuccessfulConnect;
    HealthStatus status;
    String message;
};

struct SystemHealth {
    bool healthy;
    unsigned long uptime;
    uint32_t bootCount;
    bool lastBootWasCrash;
    float temperature;
    unsigned long watchdogResets;
    HealthStatus status;
    String message;
};

struct DisplayHealth {
    bool initialized;
    int brightness;
    unsigned long lastUpdate;
    HealthStatus status;
    String message;
};

// Overall device health report
struct DeviceHealthReport {
    HealthStatus overallStatus;
    String overallMessage;
    MemoryHealth memory;
    NetworkHealth network;
    MQTTHealth mqtt;
    SystemHealth system;
    DisplayHealth display;
    unsigned long timestamp;
    
    // Issue counters
    int criticalIssues;
    int warnings;
    
    // Recommendations array
    String recommendations[10];
    int recommendationCount;
};

class DeviceHealthAnalyzer {
private:
    static unsigned long networkDisconnectCount;
    static unsigned long mqttReconnectCount;
    static unsigned long watchdogResetCount;
    
    // Analysis methods
    MemoryHealth analyzeMemory();
    NetworkHealth analyzeNetwork();
    MQTTHealth analyzeMQTT();
    SystemHealth analyzeSystem();
    DisplayHealth analyzeDisplay();
    
    // Helper methods
    HealthStatus determineOverallStatus(const DeviceHealthReport& report);
    void addRecommendation(DeviceHealthReport& report, const String& recommendation);
    const char* healthStatusToString(HealthStatus status);
    const char* healthStatusToColor(HealthStatus status);

public:
    DeviceHealthAnalyzer();
    
    // Generate comprehensive health report
    DeviceHealthReport generateReport();
    
    // Print health report to serial/log
    void printReport(const DeviceHealthReport& report);
    
    // Get health report as JSON
    String getReportJSON(const DeviceHealthReport& report);
    
    // Track events for health analysis
    static void recordNetworkDisconnect();
    static void recordMQTTReconnect();
    static void recordWatchdogReset();
    
    // Quick health checks
    bool isMemoryHealthy();
    bool isSystemHealthy();
    HealthStatus getQuickStatus();
};

// Global instance
extern DeviceHealthAnalyzer deviceHealth;

#endif // DEVICE_HEALTH_H
