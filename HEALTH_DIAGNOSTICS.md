# Device Health Diagnostics

## Overview

The ESP32-P4 AllSky Display includes a comprehensive device health monitoring and diagnostics system that analyzes various aspects of device operation and provides actionable recommendations for improvement.

## Features

### Health Monitoring Components

1. **Memory Health**
   - Tracks heap and PSRAM usage
   - Monitors minimum free memory levels
   - Detects memory fragmentation
   - Alerts on critical memory conditions

2. **Network Health**
   - WiFi signal strength (RSSI) monitoring
   - Disconnect event tracking
   - Connection stability analysis
   - Recommendations for signal improvement

3. **MQTT Health**
   - Broker connection monitoring
   - Reconnection attempt tracking
   - Stability assessment
   - Configuration validation

4. **System Health**
   - Crash detection and boot counting
   - CPU temperature monitoring
   - Watchdog reset tracking
   - Overall system stability assessment

5. **Display Health**
   - Initialization status
   - Brightness level monitoring
   - Update tracking

### Health Status Levels

The system uses five health status levels:

- **EXCELLENT** (🟢) - All systems optimal
- **GOOD** (🔵) - Minor issues, fully functional
- **WARNING** (🟡) - Issues detected, may need attention
- **CRITICAL** (🟠) - Critical issues, action required
- **FAILING** (🔴) - System failing or unstable

## Accessing Health Diagnostics

### Serial Command

Press `G` in the serial console (9600 baud) to display a comprehensive health report:

```
========================================
       DEVICE HEALTH REPORT
========================================
Overall Status: GOOD - Device is functional with minor issues
Critical Issues: 0 | Warnings: 1
----------------------------------------
[MEMORY] EXCELLENT - Memory levels optimal
  Heap: 400000 / 732160 bytes (45.3% used, min: 380000)
  PSRAM: 15000000 / 31457280 bytes (52.3% used, min: 14800000)
[NETWORK] GOOD - WiFi signal moderate
  Connected: Yes | RSSI: -68 dBm | Disconnects: 2
[MQTT] EXCELLENT - MQTT stable
  Connected: Yes | Reconnects: 1
[SYSTEM] EXCELLENT - System stable
  Uptime: 8.8 hours | Boots: 5 | Last crash: No
  Temperature: 32.5°C | Watchdog resets: 0
[DISPLAY] EXCELLENT - Display operating normally
  Brightness: 80%
----------------------------------------
RECOMMENDATIONS:
  1. Improve WiFi signal by relocating device or access point
========================================
```

### REST API Endpoint

**Endpoint:** `GET /api/health`

**Example Request:**
```bash
curl http://allskyesp32.lan:8080/api/health
```

**Example Response:**
```json
{
  "overall": {
    "status": "GOOD",
    "message": "Device is functional with minor issues",
    "critical_issues": 0,
    "warnings": 1,
    "timestamp": 31568000
  },
  "memory": {
    "status": "EXCELLENT",
    "message": "Memory levels optimal",
    "free_heap": 400000,
    "total_heap": 732160,
    "min_free_heap": 380000,
    "heap_usage_percent": 45.3,
    "free_psram": 15000000,
    "total_psram": 31457280,
    "min_free_psram": 14800000,
    "psram_usage_percent": 52.3
  },
  "network": {
    "status": "GOOD",
    "message": "WiFi signal moderate",
    "connected": true,
    "rssi": -68,
    "disconnect_count": 2
  },
  "mqtt": {
    "status": "EXCELLENT",
    "message": "MQTT stable",
    "connected": true,
    "reconnect_count": 1
  },
  "system": {
    "status": "EXCELLENT",
    "message": "System stable",
    "healthy": true,
    "uptime_ms": 31568000,
    "boot_count": 5,
    "last_boot_crash": false,
    "temperature": 32.5,
    "watchdog_resets": 0
  },
  "display": {
    "status": "EXCELLENT",
    "message": "Display operating normally",
    "initialized": true,
    "brightness": 80
  },
  "recommendations": [
    "Improve WiFi signal by relocating device or access point"
  ]
}
```

### Web Interface

The health diagnostics are accessible via the web interface:

1. Navigate to `http://allskyesp32.lan:8080/api-reference`
2. Find the `/api/health` endpoint documentation
3. Click the example link or use a REST client to query the endpoint

## Health Recommendations

The system provides actionable recommendations based on detected issues:

### Memory Issues
- Reduce image buffer sizes or max scale factor
- Reduce update frequency to minimize fragmentation
- Check for memory leaks if fragmentation increases over time

### Network Issues
- Relocate device or WiFi access point for better signal
- Check WiFi credentials if disconnected
- Investigate router logs for frequent disconnections

### MQTT Issues
- Verify MQTT broker availability
- Check MQTT credentials
- Increase reconnect interval if broker is unstable

### System Issues
- Review crash logs at `/console` page
- Improve device ventilation for high temperatures
- Check power supply stability for high boot counts

## Event Tracking

The health system automatically tracks events throughout device operation:

- **Network Disconnects**: Tracked in `network_manager.cpp` when WiFi connection is lost
- **MQTT Reconnects**: Tracked in `mqtt_manager.cpp` on each reconnection attempt
- **Watchdog Resets**: Can be tracked via `DeviceHealthAnalyzer::recordWatchdogReset()`

## Integration in Your Code

To integrate health tracking in custom code:

```cpp
#include "device_health.h"

// Track custom events
DeviceHealthAnalyzer::recordNetworkDisconnect();
DeviceHealthAnalyzer::recordMQTTReconnect();
DeviceHealthAnalyzer::recordWatchdogReset();

// Generate health report
DeviceHealthReport report = deviceHealth.generateReport();

// Print to serial/log
deviceHealth.printReport(report);

// Get JSON for API
String json = deviceHealth.getReportJSON(report);

// Quick health checks
bool memoryOk = deviceHealth.isMemoryHealthy();
bool systemOk = deviceHealth.isSystemHealthy();
HealthStatus status = deviceHealth.getQuickStatus();
```

## Troubleshooting

### High Memory Usage
If memory status shows WARNING or CRITICAL:
1. Check `/api/info` endpoint for current memory levels
2. Reduce `SCALED_BUFFER_MULTIPLIER` in `config.h`
3. Decrease image update frequency
4. Monitor fragmentation by checking min_free vs current free

### Network Instability
If network shows high disconnect count:
1. Check WiFi signal strength (RSSI)
2. Review router logs
3. Consider WiFi extender or relocating device
4. Check for interference from other devices

### Frequent MQTT Reconnects
If MQTT shows high reconnect count:
1. Verify broker is running and accessible
2. Check network stability (WiFi might be the root cause)
3. Review MQTT broker logs
4. Increase reconnect interval in advanced settings

### System Crashes
If last boot was a crash:
1. Press `G` to see health report
2. Navigate to `/console` page to view crash logs
3. Check temperature if system is overheating
4. Review recent code changes or new integrations

## API Reference

Full API documentation available at: `http://allskyesp32.lan:8080/api-reference`

## See Also

- [CRASH_DIAGNOSIS.md](CRASH_DIAGNOSIS.md) - Detailed crash analysis guide
- [OTA_GUIDE.md](OTA_GUIDE.md) - OTA update procedures
- [MANUAL_SETUP.md](MANUAL_SETUP.md) - Advanced configuration
