#include "device_health.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "display_manager.h"
#include "crash_logger.h"
#include "config_storage.h"
#include "logging.h"
#include <WiFi.h>

// Global instance
DeviceHealthAnalyzer deviceHealth;

// Static member initialization
unsigned long DeviceHealthAnalyzer::networkDisconnectCount = 0;
unsigned long DeviceHealthAnalyzer::mqttReconnectCount = 0;
unsigned long DeviceHealthAnalyzer::watchdogResetCount = 0;

DeviceHealthAnalyzer::DeviceHealthAnalyzer() {
}

MemoryHealth DeviceHealthAnalyzer::analyzeMemory() {
    MemoryHealth health;
    
    health.freeHeap = systemMonitor.getCurrentFreeHeap();
    health.totalHeap = ESP.getHeapSize();
    health.minFreeHeap = systemMonitor.getMinFreeHeap();
    health.freePsram = systemMonitor.getCurrentFreePsram();
    health.totalPsram = ESP.getPsramSize();
    health.minFreePsram = systemMonitor.getMinFreePsram();
    
    // Calculate usage percentages
    health.heapUsagePercent = 100.0 - ((float)health.freeHeap / (float)health.totalHeap * 100.0);
    health.psramUsagePercent = 100.0 - ((float)health.freePsram / (float)health.totalPsram * 100.0);
    
    // Determine status based on free memory and minimum reached
    if (health.freeHeap < configStorage.getCriticalHeapThreshold() || 
        health.freePsram < configStorage.getCriticalPSRAMThreshold()) {
        health.status = HEALTH_CRITICAL;
        health.message = "Critical: Memory critically low!";
    } else if (health.minFreeHeap < configStorage.getCriticalHeapThreshold() * 1.5 ||
               health.minFreePsram < configStorage.getCriticalPSRAMThreshold() * 1.5) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: Memory has been low during operation";
    } else if (health.heapUsagePercent > 80.0 || health.psramUsagePercent > 80.0) {
        health.status = HEALTH_GOOD;
        health.message = "Good: Memory usage is high but stable";
    } else {
        health.status = HEALTH_EXCELLENT;
        health.message = "Excellent: Memory levels optimal";
    }
    
    return health;
}

NetworkHealth DeviceHealthAnalyzer::analyzeNetwork() {
    NetworkHealth health;
    
    health.connected = wifiManager.isConnected();
    health.rssi = WiFi.RSSI();
    health.uptime = millis();
    health.disconnectCount = networkDisconnectCount;
    
    if (!health.connected) {
        health.status = HEALTH_FAILING;
        health.message = "Critical: WiFi disconnected";
    } else if (health.rssi < -80) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: Weak WiFi signal (RSSI < -80 dBm)";
    } else if (health.rssi < -70) {
        health.status = HEALTH_GOOD;
        health.message = "Good: WiFi signal moderate";
    } else if (health.disconnectCount > 10) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: Frequent WiFi disconnections detected";
    } else if (health.disconnectCount > 5) {
        health.status = HEALTH_GOOD;
        health.message = "Good: Some WiFi disconnections, but stable";
    } else {
        health.status = HEALTH_EXCELLENT;
        health.message = "Excellent: WiFi stable with strong signal";
    }
    
    return health;
}

MQTTHealth DeviceHealthAnalyzer::analyzeMQTT() {
    MQTTHealth health;
    
    health.connected = mqttManager.isConnected();
    health.reconnectCount = mqttReconnectCount;
    health.lastSuccessfulConnect = millis(); // Simplified - could track actual time
    
    if (!configStorage.getMQTTServer().isEmpty()) {
        if (!health.connected) {
            health.status = HEALTH_CRITICAL;
            health.message = "Critical: MQTT disconnected (server configured)";
        } else if (health.reconnectCount > 20) {
            health.status = HEALTH_WARNING;
            health.message = "Warning: Frequent MQTT reconnections";
        } else if (health.reconnectCount > 10) {
            health.status = HEALTH_GOOD;
            health.message = "Good: MQTT connected with some reconnects";
        } else {
            health.status = HEALTH_EXCELLENT;
            health.message = "Excellent: MQTT stable";
        }
    } else {
        health.status = HEALTH_EXCELLENT;
        health.message = "Not configured (optional)";
    }
    
    return health;
}

SystemHealth DeviceHealthAnalyzer::analyzeSystem() {
    SystemHealth health;
    
    health.healthy = systemMonitor.isSystemHealthy();
    health.uptime = millis();
    health.bootCount = crashLogger.getBootCount();
    health.lastBootWasCrash = crashLogger.wasLastBootCrash();
    health.temperature = temperatureRead();
    health.watchdogResets = watchdogResetCount;
    
    if (!health.healthy) {
        health.status = HEALTH_CRITICAL;
        health.message = "Critical: System health check failed";
    } else if (health.lastBootWasCrash) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: Previous boot was a crash";
    } else if (health.temperature > 80.0) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: High CPU temperature";
    } else if (health.bootCount > 50) {
        health.status = HEALTH_GOOD;
        health.message = "Good: High boot count (possible instability history)";
    } else if (health.watchdogResets > 5) {
        health.status = HEALTH_GOOD;
        health.message = "Good: Some watchdog resets detected";
    } else {
        health.status = HEALTH_EXCELLENT;
        health.message = "Excellent: System stable";
    }
    
    return health;
}

DisplayHealth DeviceHealthAnalyzer::analyzeDisplay() {
    DisplayHealth health;
    
    health.initialized = true; // Assume initialized if we're running
    health.brightness = displayManager.getBrightness();
    health.lastUpdate = millis(); // Simplified
    
    if (!health.initialized) {
        health.status = HEALTH_FAILING;
        health.message = "Critical: Display not initialized";
    } else if (health.brightness < 10) {
        health.status = HEALTH_WARNING;
        health.message = "Warning: Display brightness very low";
    } else {
        health.status = HEALTH_EXCELLENT;
        health.message = "Excellent: Display operating normally";
    }
    
    return health;
}

HealthStatus DeviceHealthAnalyzer::determineOverallStatus(const DeviceHealthReport& report) {
    // Overall status is the worst of all component statuses
    HealthStatus worst = HEALTH_EXCELLENT;
    
    if (report.memory.status > worst) worst = report.memory.status;
    if (report.network.status > worst) worst = report.network.status;
    if (report.mqtt.status > worst) worst = report.mqtt.status;
    if (report.system.status > worst) worst = report.system.status;
    if (report.display.status > worst) worst = report.display.status;
    
    return worst;
}

void DeviceHealthAnalyzer::addRecommendation(DeviceHealthReport& report, const String& recommendation) {
    if (report.recommendationCount < 10) {
        report.recommendations[report.recommendationCount++] = recommendation;
    }
}

const char* DeviceHealthAnalyzer::healthStatusToString(HealthStatus status) {
    switch (status) {
        case HEALTH_EXCELLENT: return "EXCELLENT";
        case HEALTH_GOOD: return "GOOD";
        case HEALTH_WARNING: return "WARNING";
        case HEALTH_CRITICAL: return "CRITICAL";
        case HEALTH_FAILING: return "FAILING";
        default: return "UNKNOWN";
    }
}

const char* DeviceHealthAnalyzer::healthStatusToColor(HealthStatus status) {
    switch (status) {
        case HEALTH_EXCELLENT: return "GREEN";
        case HEALTH_GOOD: return "CYAN";
        case HEALTH_WARNING: return "YELLOW";
        case HEALTH_CRITICAL: return "ORANGE";
        case HEALTH_FAILING: return "RED";
        default: return "WHITE";
    }
}

DeviceHealthReport DeviceHealthAnalyzer::generateReport() {
    DeviceHealthReport report;
    
    report.timestamp = millis();
    report.criticalIssues = 0;
    report.warnings = 0;
    report.recommendationCount = 0;
    
    // Analyze each component
    report.memory = analyzeMemory();
    report.network = analyzeNetwork();
    report.mqtt = analyzeMQTT();
    report.system = analyzeSystem();
    report.display = analyzeDisplay();
    
    // Count issues
    if (report.memory.status >= HEALTH_CRITICAL) report.criticalIssues++;
    else if (report.memory.status >= HEALTH_WARNING) report.warnings++;
    
    if (report.network.status >= HEALTH_CRITICAL) report.criticalIssues++;
    else if (report.network.status >= HEALTH_WARNING) report.warnings++;
    
    if (report.mqtt.status >= HEALTH_CRITICAL) report.criticalIssues++;
    else if (report.mqtt.status >= HEALTH_WARNING) report.warnings++;
    
    if (report.system.status >= HEALTH_CRITICAL) report.criticalIssues++;
    else if (report.system.status >= HEALTH_WARNING) report.warnings++;
    
    if (report.display.status >= HEALTH_CRITICAL) report.criticalIssues++;
    else if (report.display.status >= HEALTH_WARNING) report.warnings++;
    
    // Generate recommendations based on findings
    if (report.memory.status >= HEALTH_WARNING) {
        addRecommendation(report, "Reduce image buffer sizes or reduce max scale factor in config.h");
    }
    
    if (report.memory.minFreeHeap < report.memory.totalHeap * 0.1) {
        addRecommendation(report, "Memory fragmentation possible - consider reducing update frequency");
    }
    
    if (report.network.status >= HEALTH_WARNING && report.network.connected) {
        addRecommendation(report, "Improve WiFi signal by relocating device or access point");
    }
    
    if (!report.network.connected) {
        addRecommendation(report, "Check WiFi credentials and router availability");
    }
    
    if (report.network.disconnectCount > 10) {
        addRecommendation(report, "Investigate network stability - check router logs");
    }
    
    if (report.mqtt.status >= HEALTH_WARNING && !configStorage.getMQTTServer().isEmpty()) {
        addRecommendation(report, "Check MQTT broker availability and credentials");
    }
    
    if (report.system.lastBootWasCrash) {
        addRecommendation(report, "Check crash logs for root cause - see /console page");
    }
    
    if (report.system.temperature > 80.0) {
        addRecommendation(report, "Improve device ventilation to reduce CPU temperature");
    }
    
    if (report.system.bootCount > 100) {
        addRecommendation(report, "High boot count may indicate power issues or instability");
    }
    
    if (report.mqtt.reconnectCount > 20) {
        addRecommendation(report, "Increase MQTT reconnect interval or check broker stability");
    }
    
    // Determine overall status
    report.overallStatus = determineOverallStatus(report);
    
    // Set overall message
    if (report.overallStatus == HEALTH_EXCELLENT) {
        report.overallMessage = "Device is operating optimally";
    } else if (report.overallStatus == HEALTH_GOOD) {
        report.overallMessage = "Device is functional with minor issues";
    } else if (report.overallStatus == HEALTH_WARNING) {
        report.overallMessage = "Device has issues that need attention";
    } else if (report.overallStatus == HEALTH_CRITICAL) {
        report.overallMessage = "Device has critical issues requiring immediate action";
    } else {
        report.overallMessage = "Device is failing or unstable";
    }
    
    report.recommendations = report.recommendationCount;
    
    return report;
}

void DeviceHealthAnalyzer::printReport(const DeviceHealthReport& report) {
    LOG_INFO("========================================");
    LOG_INFO("       DEVICE HEALTH REPORT");
    LOG_INFO("========================================");
    LOG_INFO_F("Overall Status: %s - %s\n", healthStatusToString(report.overallStatus), report.overallMessage.c_str());
    LOG_INFO_F("Critical Issues: %d | Warnings: %d\n", report.criticalIssues, report.warnings);
    LOG_INFO("----------------------------------------");
    
    // Memory
    LOG_INFO_F("[MEMORY] %s - %s\n", healthStatusToString(report.memory.status), report.memory.message.c_str());
    LOG_INFO_F("  Heap: %d / %d bytes (%.1f%% used, min: %d)\n", 
               report.memory.freeHeap, report.memory.totalHeap, 
               report.memory.heapUsagePercent, report.memory.minFreeHeap);
    LOG_INFO_F("  PSRAM: %d / %d bytes (%.1f%% used, min: %d)\n",
               report.memory.freePsram, report.memory.totalPsram,
               report.memory.psramUsagePercent, report.memory.minFreePsram);
    
    // Network
    LOG_INFO_F("[NETWORK] %s - %s\n", healthStatusToString(report.network.status), report.network.message.c_str());
    LOG_INFO_F("  Connected: %s | RSSI: %d dBm | Disconnects: %lu\n",
               report.network.connected ? "Yes" : "No",
               report.network.rssi, report.network.disconnectCount);
    
    // MQTT
    LOG_INFO_F("[MQTT] %s - %s\n", healthStatusToString(report.mqtt.status), report.mqtt.message.c_str());
    LOG_INFO_F("  Connected: %s | Reconnects: %lu\n",
               report.mqtt.connected ? "Yes" : "No",
               report.mqtt.reconnectCount);
    
    // System
    LOG_INFO_F("[SYSTEM] %s - %s\n", healthStatusToString(report.system.status), report.system.message.c_str());
    LOG_INFO_F("  Uptime: %.1f hours | Boots: %lu | Last crash: %s\n",
               report.system.uptime / 3600000.0, report.system.bootCount,
               report.system.lastBootWasCrash ? "Yes" : "No");
    LOG_INFO_F("  Temperature: %.1f°C | Watchdog resets: %lu\n",
               report.system.temperature, report.system.watchdogResets);
    
    // Display
    LOG_INFO_F("[DISPLAY] %s - %s\n", healthStatusToString(report.display.status), report.display.message.c_str());
    LOG_INFO_F("  Brightness: %d%%\n", report.display.brightness);
    
    // Recommendations
    if (report.recommendationCount > 0) {
        LOG_INFO("----------------------------------------");
        LOG_INFO("RECOMMENDATIONS:");
        for (int i = 0; i < report.recommendationCount; i++) {
            LOG_INFO_F("  %d. %s\n", i + 1, report.recommendations[i].c_str());
        }
    }
    
    LOG_INFO("========================================");
}

String DeviceHealthAnalyzer::getReportJSON(const DeviceHealthReport& report) {
    String json = "{";
    
    // Overall status
    json += "\"overall\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.overallStatus)) + "\",";
    json += "\"message\":\"" + report.overallMessage + "\",";
    json += "\"critical_issues\":" + String(report.criticalIssues) + ",";
    json += "\"warnings\":" + String(report.warnings) + ",";
    json += "\"timestamp\":" + String(report.timestamp);
    json += "},";
    
    // Memory health
    json += "\"memory\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.memory.status)) + "\",";
    json += "\"message\":\"" + report.memory.message + "\",";
    json += "\"free_heap\":" + String(report.memory.freeHeap) + ",";
    json += "\"total_heap\":" + String(report.memory.totalHeap) + ",";
    json += "\"min_free_heap\":" + String(report.memory.minFreeHeap) + ",";
    json += "\"heap_usage_percent\":" + String(report.memory.heapUsagePercent, 1) + ",";
    json += "\"free_psram\":" + String(report.memory.freePsram) + ",";
    json += "\"total_psram\":" + String(report.memory.totalPsram) + ",";
    json += "\"min_free_psram\":" + String(report.memory.minFreePsram) + ",";
    json += "\"psram_usage_percent\":" + String(report.memory.psramUsagePercent, 1);
    json += "},";
    
    // Network health
    json += "\"network\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.network.status)) + "\",";
    json += "\"message\":\"" + report.network.message + "\",";
    json += "\"connected\":" + String(report.network.connected ? "true" : "false") + ",";
    json += "\"rssi\":" + String(report.network.rssi) + ",";
    json += "\"disconnect_count\":" + String(report.network.disconnectCount);
    json += "},";
    
    // MQTT health
    json += "\"mqtt\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.mqtt.status)) + "\",";
    json += "\"message\":\"" + report.mqtt.message + "\",";
    json += "\"connected\":" + String(report.mqtt.connected ? "true" : "false") + ",";
    json += "\"reconnect_count\":" + String(report.mqtt.reconnectCount);
    json += "},";
    
    // System health
    json += "\"system\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.system.status)) + "\",";
    json += "\"message\":\"" + report.system.message + "\",";
    json += "\"healthy\":" + String(report.system.healthy ? "true" : "false") + ",";
    json += "\"uptime_ms\":" + String(report.system.uptime) + ",";
    json += "\"boot_count\":" + String(report.system.bootCount) + ",";
    json += "\"last_boot_crash\":" + String(report.system.lastBootWasCrash ? "true" : "false") + ",";
    json += "\"temperature\":" + String(report.system.temperature, 1) + ",";
    json += "\"watchdog_resets\":" + String(report.system.watchdogResets);
    json += "},";
    
    // Display health
    json += "\"display\":{";
    json += "\"status\":\"" + String(healthStatusToString(report.display.status)) + "\",";
    json += "\"message\":\"" + report.display.message + "\",";
    json += "\"initialized\":" + String(report.display.initialized ? "true" : "false") + ",";
    json += "\"brightness\":" + String(report.display.brightness);
    json += "},";
    
    // Recommendations
    json += "\"recommendations\":[";
    for (int i = 0; i < report.recommendationCount; i++) {
        if (i > 0) json += ",";
        json += "\"" + report.recommendations[i] + "\"";
    }
    json += "]";
    
    json += "}";
    
    return json;
}

void DeviceHealthAnalyzer::recordNetworkDisconnect() {
    networkDisconnectCount++;
    LOG_DEBUG_F("[HealthTracker] Network disconnect count: %lu\n", networkDisconnectCount);
}

void DeviceHealthAnalyzer::recordMQTTReconnect() {
    mqttReconnectCount++;
    LOG_DEBUG_F("[HealthTracker] MQTT reconnect count: %lu\n", mqttReconnectCount);
}

void DeviceHealthAnalyzer::recordWatchdogReset() {
    watchdogResetCount++;
    LOG_DEBUG_F("[HealthTracker] Watchdog reset count: %lu\n", watchdogResetCount);
}

bool DeviceHealthAnalyzer::isMemoryHealthy() {
    MemoryHealth health = analyzeMemory();
    return health.status <= HEALTH_GOOD;
}

bool DeviceHealthAnalyzer::isSystemHealthy() {
    SystemHealth health = analyzeSystem();
    return health.status <= HEALTH_GOOD;
}

HealthStatus DeviceHealthAnalyzer::getQuickStatus() {
    DeviceHealthReport report = generateReport();
    return report.overallStatus;
}
