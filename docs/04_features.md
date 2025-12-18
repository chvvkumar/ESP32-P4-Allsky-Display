# Features Guide

Comprehensive guide to all features and capabilities of the ESP32-P4 AllSky Display firmware.

## Table of Contents
- [Core Features](#core-features)
- [Multi-Image Cycling](#multi-image-cycling)
- [Hardware Acceleration](#hardware-acceleration)
- [Touch Controls](#touch-controls)
- [Serial Commands](#serial-commands)
- [Health Diagnostics](#health-diagnostics)
- [Health API Examples](#health-api-examples)
- [Home Assistant Integration](#home-assistant-integration)
- [Web Interface](#web-interface)

---

## Core Features

### High-Resolution Image Display

**Supported Image Sizes:**

| Configuration | Max Image Dimensions | Max File Size | Buffer Size | Recommended Use |
|--------------|---------------------|---------------|-------------|-----------------|
| **Current (Default)** | **1448×1448 pixels** | **~4MB** | 4MB | High-resolution AllSky images |
| Conservative | 1024×1024 pixels | ~2MB | 2MB | Standard quality |
| Legacy | 512×512 pixels | ~512KB | 1MB | Low memory systems |

**Current Setup:** The firmware is configured to support images up to **1448×1448 pixels** with hardware-accelerated scaling up to **2.0×** (1600×1600 output).

**Format Support:**
- ✅ **JPEG** - Full support via JPEGDEC library
- ❌ **PNG** - Not supported (convert to JPEG at source)
- ❌ **WebP, GIF, BMP** - Not supported

**Image Processing Pipeline:**
```
HTTP Download → JPEG Decode → RGB565 Conversion → PPA Transform → Display
     (1-30s)       (100-300ms)      (included)      (385-507ms)    (instant)
```

### Double Buffering

**Flicker-Free Updates:**
- Download to `pendingFullImageBuffer` while `fullImageBuffer` is displayed
- Atomic buffer swap after download completes
- No screen clearing between images (after first image)

**Memory Layout:**
```
imageBuffer (1.28MB)           - HTTP download scratch
fullImageBuffer (4MB)          - Currently displayed image
pendingFullImageBuffer (4MB)   - Next image being prepared
scaledBuffer (5.12MB)          - PPA transformation output
```

---

## Multi-Image Cycling

### Overview

Display up to **10 different image sources** with automatic cycling or API-triggered refresh.

**Features:**
- Two update modes: Automatic Cycling or API-Triggered Refresh (v-snd-0.62+)
- Configurable cycle interval (10 seconds - 1 hour)
- Random or sequential order
- Per-image transform settings (scale, offset, rotation)
- Individual enable/disable for each source
- Manual advance via touch, serial command, or API call

### Configuration

**Via Web UI:**
1. Navigate to `http://allskyesp32.lan:8080/image`
2. Select "Update Mode":
   - **⏺ Automatic Cycling**: Auto-cycles at configured intervals
   - **🔗 API-Triggered Refresh**: Manual control via `/api/force-refresh`
3. Enable "Multi-Image Cycling" (for automatic mode)
4. Click "Add Image Source" for each URL
5. Set cycle interval slider (automatic mode)
6. Enable "Random Order" if desired (automatic mode)
7. Configure per-image transforms
8. Click "Save Configuration"

**Per-Image Settings:**
```
Image 1: 
  URL: https://camera1.example.com/allsky.jpg
  Scale: 1.5×
  Offset: +50, +0
  Rotation: 0°
  Enabled: Yes

Image 2:
  URL: https://camera2.example.com/current.jpg
  Scale: 1.2×
  Offset: -20, +30
  Rotation: 90°
  Enabled: Yes

Image 3:
  URL: https://camera3.example.com/sky.jpg
  Scale: 1.8×
  Offset: 0, -50
  Rotation: 180°
  Enabled: No
```

### Cycling Modes

**Sequential Mode:**
- Images displayed in order: 1 → 2 → 3 → ... → N → 1
- Predictable, easy to understand
- Good for monitoring specific sequence

**Random Mode:**
- Images selected randomly from enabled sources
- More variety, prevents pattern blindness
- Good for diverse camera feeds

**API-Triggered Refresh Mode:** (v-snd-0.62+)
- Cycling disabled, image only updates when `/api/force-refresh` is called
- Use REST API or MQTT to control when image refreshes
- Touch gestures still work for manual cycling
- Ideal for external automation systems or manual control
- Example use cases:
  - Refresh only when new satellite image available
  - Update on motion detection events
  - Synchronized with external triggers

---

## Hardware Acceleration

### ESP32-P4 PPA (Pixel Processing Accelerator)

**Capabilities:**
- Hardware-accelerated image scaling (nearest-neighbor or bilinear)
- Hardware rotation (0°, 90°, 180°, 270° only)
- DMA-based transfers (no CPU involvement)
- Async operation with status checking

**Performance Comparison:**

| Operation | Hardware PPA | Software Fallback | Speedup |
|-----------|-------------|-------------------|---------|
| Scale 1.5× | 385-420ms | 2800-3200ms | **7-8× faster** |
| Scale 2.0× | 450-507ms | 3500-4000ms | **7-8× faster** |
| Rotate 90° | 320-380ms | 2400-2800ms | **7-8× faster** |

**Automatic Fallback:**
- PPA unavailable (init failed) → Software rendering
- Image too large for buffer → Software rendering
- Non-90° rotation → Software rotation

**DMA Alignment Requirements:**
- Source and destination buffers must be 64-byte aligned
- Allocated with `heap_caps_aligned_alloc(64, size, MALLOC_CAP_SPIRAM)`
- Failure to align → Crash (StoreProhibited exception)

### Transform Pipeline

**Combined Transform Order:**
```
1. Scale (hardware or software)
2. Rotate (hardware 90° multiples or software for arbitrary angles)
3. Offset (crop to display coordinates)
4. Copy to display framebuffer via draw16bitRGBBitmap()
```

**Example:**
```
Original: 1200×1200 fisheye image
↓ Scale 1.5× (PPA)
1800×1800 scaled image
↓ Rotate 0° (no-op)
1800×1800 rotated image
↓ Offset +0,+0 (center crop)
800×800 display region
↓ Copy to framebuffer
Display shows centered, scaled fisheye
```

---

## Touch Controls

### GT911 Touch Controller

**Hardware:**
- Capacitive touch controller via I2C
- Supports up to 5 simultaneous touch points
- Configurable I2C pins per display model

**I2C Configuration:**
```cpp
// 3.4" display
.i2c_sda_pin = 8,
.i2c_scl_pin = 18,

// 4.0" display
.i2c_sda_pin = 8,
.i2c_scl_pin = 9,
```

### Gesture Recognition

**Single Tap:**
- **Action:** Advance to next image
- **Timing:** Touch duration 30-500ms
- **Debounce:** 200ms between taps

**Double Tap:**
- **Action:** Toggle cycling / single-refresh mode
- **Timing:** Two taps within 600ms
- **Feedback:** Mode change logged to serial/console

**Touch State Machine:**
```
IDLE → Touch Down → TOUCHED
       ↓
    Touch Up (30-500ms) → TAP_DETECTED
       ↓
    Second Touch (within 600ms) → DOUBLE_TAP_DETECTED
       ↓
    No Second Touch (>600ms) → SINGLE_TAP_CONFIRMED
```

**Gesture Parameters (Tunable):**
```cpp
#define DOUBLE_TAP_TIMEOUT_MS 600   // Max time between taps
#define MIN_TAP_DURATION_MS 30      // Min touch duration
#define MAX_TAP_DURATION_MS 500     // Max touch duration
#define TOUCH_DEBOUNCE_MS 200       // Debounce between gestures
```

---

## Serial Commands

### Command Reference

Connect via Serial Monitor at **9600 baud** to access these commands:

**Navigation & Image Control:**
```
N : Next image (force update, reset cycle)
F : Refresh current image (download again)
T : Toggle cycling mode (on/off)
D : Download image now (manual trigger)
```

**Scale & Transform:**
```
+/- : Increase/Decrease scale by 0.1
W   : Move image up (offset Y -10px)
A   : Move image left (offset X -10px)
S   : Move image down (offset Y +10px)
D   : Move image right (offset X +10px)
Q   : Rotate counter-clockwise (-90°)
E   : Rotate clockwise (+90°)
R   : Reset all transforms to defaults
X   : Reset transforms (same as R)
```

**Display Control:**
```
L   : Increase brightness (+10%)
K   : Decrease brightness (-10%)
```

**System Information:**
```
I   : System information (uptime, version, config)
M   : Memory status (heap/PSRAM current and minimum)
V   : Version info (firmware, git commit, build date)
N   : Network status (IP, signal, MQTT connection)
S   : Complete system status (all info combined)
G   : Device health diagnostics report
H   : Help (show all commands)
?   : Help (same as H)
```

**System Actions:**
```
B   : Reboot device
F   : Factory reset (clear all NVS config)
C   : Clear crash logs from NVS
```

### Logging System

**Severity Levels:**
```cpp
LOG_DEBUG("Verbose debugging message");
LOG_INFO("General information");
LOG_WARNING("Non-critical warning");
LOG_ERROR("Error occurred but continuing");
LOG_CRITICAL("Critical failure!");

// With printf-style formatting
LOG_INFO_F("Image size: %dx%d", width, height);
LOG_ERROR_F("HTTP error code: %d", httpCode);
```

**Output Destinations:**
- Serial Monitor (USB, 9600 baud)
- WebSocket Console (WiFi, port 81)

**Severity Filtering:**
- Set via Web UI: Console → Severity buttons
- Or NVS: `configStorage.setMinLogSeverity(LOG_WARNING)`
- Only messages ≥ severity level are shown

---

## Health Diagnostics

### Overview

Comprehensive device health monitoring and diagnostics system that analyzes multiple components and provides actionable recommendations.

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

### Accessing Health Diagnostics

**Serial Command:**

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

**REST API Endpoint:**

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

**Web Interface:**

The health diagnostics are accessible via the web interface:

1. Navigate to `http://allskyesp32.lan:8080/api-reference`
2. Find the `/api/health` endpoint documentation
3. Click the example link or use a REST client to query the endpoint

### Health Recommendations

The system provides actionable recommendations based on detected issues:

**Memory Issues:**
- Reduce image buffer sizes or max scale factor
- Reduce update frequency to minimize fragmentation
- Check for memory leaks if fragmentation increases over time

**Network Issues:**
- Relocate device or WiFi access point for better signal
- Check WiFi credentials if disconnected
- Investigate router logs for frequent disconnections

**MQTT Issues:**
- Verify MQTT broker availability
- Check MQTT credentials
- Increase reconnect interval if broker is unstable

**System Issues:**
- Review crash logs at `/console` page
- Improve device ventilation for high temperatures
- Check power supply stability for high boot counts

---

## Health API Examples

### Command Line Examples

**Basic Health Check:**
```bash
# Get health report
curl http://allskyesp32.lan:8080/api/health

# Pretty print JSON response
curl -s http://allskyesp32.lan:8080/api/health | python3 -m json.tool

# Save to file
curl -s http://allskyesp32.lan:8080/api/health > device_health.json
```

**Extract Specific Health Metrics:**
```bash
# Get overall status only (requires jq)
curl -s http://allskyesp32.lan:8080/api/health | jq '.overall.status'

# Get memory usage percentage
curl -s http://allskyesp32.lan:8080/api/health | jq '.memory.heap_usage_percent'

# Get WiFi signal strength
curl -s http://allskyesp32.lan:8080/api/health | jq '.network.rssi'

# Get all recommendations
curl -s http://allskyesp32.lan:8080/api/health | jq '.recommendations[]'
```

**Monitoring Script Example:**
```bash
#!/bin/bash
# health_monitor.sh - Check device health every 5 minutes

DEVICE_URL="http://allskyesp32.lan:8080"
LOG_FILE="health_log.txt"

while true; do
    TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')
    STATUS=$(curl -s "$DEVICE_URL/api/health" | jq -r '.overall.status')
    TEMP=$(curl -s "$DEVICE_URL/api/health" | jq -r '.system.temperature')
    HEAP=$(curl -s "$DEVICE_URL/api/health" | jq -r '.memory.heap_usage_percent')
    
    echo "$TIMESTAMP | Status: $STATUS | Temp: ${TEMP}°C | Heap: ${HEAP}%" >> "$LOG_FILE"
    
    # Alert if critical
    if [ "$STATUS" == "CRITICAL" ] || [ "$STATUS" == "FAILING" ]; then
        echo "ALERT: Device health is $STATUS!" | mail -s "ESP32 Health Alert" admin@example.com
    fi
    
    sleep 300  # 5 minutes
done
```

### Python Examples

**Basic Health Check:**
```python
import requests
import json

# Get health report
response = requests.get('http://allskyesp32.lan:8080/api/health')
health = response.json()

print(f"Overall Status: {health['overall']['status']}")
print(f"Message: {health['overall']['message']}")
print(f"Critical Issues: {health['overall']['critical_issues']}")
print(f"Warnings: {health['overall']['warnings']}")

# Print recommendations
if health['recommendations']:
    print("\nRecommendations:")
    for i, rec in enumerate(health['recommendations'], 1):
        print(f"  {i}. {rec}")
```

**Health Monitoring Class:**
```python
import requests
from datetime import datetime
import time

class DeviceHealthMonitor:
    def __init__(self, device_url):
        self.device_url = device_url
        self.health_url = f"{device_url}/api/health"
        
    def get_health(self):
        """Fetch current health report"""
        try:
            response = requests.get(self.health_url, timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error fetching health: {e}")
            return None
    
    def check_memory(self, health):
        """Check memory health"""
        memory = health.get('memory', {})
        status = memory.get('status', 'UNKNOWN')
        heap_usage = memory.get('heap_usage_percent', 0)
        psram_usage = memory.get('psram_usage_percent', 0)
        
        print(f"Memory Status: {status}")
        print(f"  Heap Usage: {heap_usage:.1f}%")
        print(f"  PSRAM Usage: {psram_usage:.1f}%")
        
        return status in ['EXCELLENT', 'GOOD']
    
    def monitor_continuously(self, interval=300):
        """Monitor health continuously"""
        print(f"Starting health monitoring (interval: {interval}s)")
        
        while True:
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            print(f"\n{'='*60}")
            print(f"Health Check: {timestamp}")
            print('='*60)
            
            health = self.get_health()
            if health:
                overall = health.get('overall', {})
                print(f"Overall: {overall.get('status', 'UNKNOWN')}")
                print(f"Message: {overall.get('message', 'N/A')}")
                
                self.check_memory(health)
                
                # Show recommendations
                if health.get('recommendations'):
                    print("\nRecommendations:")
                    for i, rec in enumerate(health['recommendations'], 1):
                        print(f"  {i}. {rec}")
            
            time.sleep(interval)

# Usage
monitor = DeviceHealthMonitor('http://allskyesp32.lan:8080')
health = monitor.get_health()
if health:
    print(f"Device Status: {health['overall']['status']}")
```

### JavaScript/Node.js Examples

**Basic Health Check:**
```javascript
const axios = require('axios');

async function checkHealth() {
    try {
        const response = await axios.get('http://allskyesp32.lan:8080/api/health');
        const health = response.data;
        
        console.log('Overall Status:', health.overall.status);
        console.log('Message:', health.overall.message);
        console.log('Critical Issues:', health.overall.critical_issues);
        console.log('Warnings:', health.overall.warnings);
        
        if (health.recommendations.length > 0) {
            console.log('\nRecommendations:');
            health.recommendations.forEach((rec, i) => {
                console.log(`  ${i + 1}. ${rec}`);
            });
        }
        
        return health;
    } catch (error) {
        console.error('Error fetching health:', error.message);
        return null;
    }
}

checkHealth();
```

### Home Assistant Integration

**Configuration:**
```yaml
# configuration.yaml
rest:
  - resource: http://allskyesp32.lan:8080/api/health
    scan_interval: 300
    sensor:
      - name: "AllSky Health Status"
        value_template: "{{ value_json.overall.status }}"
        
      - name: "AllSky Memory Usage"
        value_template: "{{ value_json.memory.heap_usage_percent }}"
        unit_of_measurement: "%"
        
      - name: "AllSky Temperature"
        value_template: "{{ value_json.system.temperature }}"
        unit_of_measurement: "°C"
        device_class: temperature
        
      - name: "AllSky WiFi Signal"
        value_template: "{{ value_json.network.rssi }}"
        unit_of_measurement: "dBm"
        device_class: signal_strength

automation:
  - alias: "AllSky Health Alert"
    trigger:
      - platform: state
        entity_id: sensor.allsky_health_status
        to: "CRITICAL"
    action:
      - service: notify.notify
        data:
          title: "AllSky Camera Alert"
          message: "Device health is CRITICAL! Check the device immediately."
```

---

## Home Assistant Integration

### Auto-Discovery

**Prerequisites:**
- MQTT broker accessible from both HA and ESP32
- HA MQTT integration configured
- Device configured with correct broker details

**Entities Created:**
- **Light:** Backlight brightness control (0-100%)
- **Switch:** Cycling mode toggle, image source toggles
- **Number:** Scale X/Y, Offset X/Y sliders
- **Select:** Rotation angle dropdown (0°, 90°, 180°, 270°)
- **Button:** "Next Image" trigger, "Restart Device" trigger
- **Sensor:** Uptime, free memory, WiFi signal, current image index

**Discovery Topics:**
```
homeassistant/light/allsky_display_backlight/config
homeassistant/switch/allsky_display_cycling/config
homeassistant/number/allsky_display_scale_x/config
homeassistant/sensor/allsky_display_uptime/config
... (20+ entities total)
```

**Manual Discovery Trigger:**
If entities don't appear automatically:
1. Restart MQTT Manager via serial command `M` (triggers reconnect)
2. Check MQTT broker logs for discovery messages
3. Verify HA is subscribed to `homeassistant/#`
4. Check HA Developer Tools → MQTT for incoming messages

### MQTT Control Examples

**Brightness Control:**
```bash
# Set brightness to 75%
mosquitto_pub -h homeassistant.local \
  -t "homeassistant/allsky_display/light/command" \
  -m '{"brightness":75}'

# Turn off display (brightness 0)
mosquitto_pub -h homeassistant.local \
  -t "homeassistant/allsky_display/light/command" \
  -m '{"state":"OFF"}'
```

**Image Control:**
```bash
# Next image
mosquitto_pub -h homeassistant.local \
  -t "homeassistant/allsky_display/button/next_image/command" \
  -m 'PRESS'

# Toggle cycling
mosquitto_pub -h homeassistant.local \
  -t "homeassistant/allsky_display/switch/cycling/command" \
  -m 'ON'
```

---

## Web Interface

### Modern, Responsive UI

Access `http://allskyesp32.lan:8080/` for:
- Dark theme with toast notifications
- Mobile responsive with hamburger menu
- Keyboard shortcuts (Ctrl+S to save, Ctrl+R to restart, Ctrl+N next image)
- Real-time validation and feedback
- Auto-updating status display

**Configuration Pages:**
- **Home** - System status and quick actions
- **Console** - Real-time serial output monitoring over WiFi
- **WiFi** - Network settings
- **MQTT** - Home Assistant integration
- **Images** - Multi-image sources (up to 10)
- **Display** - Brightness and transforms
- **Advanced** - Intervals and thresholds

### WebSocket Console

**Remote Serial Monitoring:** Access `http://[device-ip]:8080/console` to view real-time serial output over WiFi

**Features:**
- No USB connection required
- Real-time log streaming via WebSocket (port 81)
- Auto-scroll, message counter, log filtering
- Download logs to file
- Connect/disconnect on demand

**Use Cases:** 
- Monitor debug output remotely
- Troubleshoot network issues
- Verify system behavior

### REST API Endpoints

All endpoints return JSON responses and use standard HTTP methods.

#### GET /api/info

**Description:** Retrieve complete system status information

**Example Request:**
```bash
curl http://allskyesp32.lan:8080/api/info
```

**Response:**
```json
{
  "uptime": 31568000,
  "wifi": {
    "connected": true,
    "rssi": -65,
    "ssid": "MyNetwork"
  },
  "mqtt": {
    "connected": true,
    "server": "192.168.1.100"
  },
  "memory": {
    "free_heap": 400000,
    "free_psram": 15000000
  },
  "image": {
    "current_index": 0,
    "total_sources": 5,
    "cycling_enabled": true,
    "update_mode": 0
  }
}
```

#### GET /api/health

**Description:** Get device health diagnostics with status indicators

**Example Request:**
```bash
curl http://allskyesp32.lan:8080/api/health
```

See [Health Diagnostics](#health-diagnostics) section for full response format.

#### GET /api/current_image

**Description:** Get current image URL and index

**Example Request:**
```bash
curl http://allskyesp32.lan:8080/api/current_image
```

**Response:**
```json
{
  "index": 0,
  "url": "https://camera1.example.com/allsky.jpg",
  "total_sources": 5
}
```

#### POST /api/next_image

**Description:** Manually advance to next image in cycling sequence

**Example Request:**
```bash
curl -X POST http://allskyesp32.lan:8080/api/next_image
```

**Response:**
```json
{
  "status": "success",
  "message": "Switched to next image and refreshed display"
}
```

#### POST /api/force-refresh

**Description:** Force immediate re-download of current image (v-snd-0.62+)

**Example Request:**
```bash
curl -X POST http://allskyesp32.lan:8080/api/force-refresh
```

**Response:**
```json
{
  "status": "success",
  "message": "Current image refreshed"
}
```

**Use Cases:**
- Refresh image on external trigger (motion detection, satellite image availability)
- Manual refresh via automation script
- Synchronized updates with external systems
- Testing image updates without waiting for cycle interval

**Example Automation (Home Assistant):**
```yaml
automation:
  - alias: "Refresh AllSky Display on New Image"
    trigger:
      platform: state
      entity_id: sensor.allsky_camera_last_update
    action:
      - service: rest_command.refresh_allsky_display
```

#### POST /api/save

**Description:** Save configuration to NVS storage

**Example Request:**
```bash
curl -X POST http://allskyesp32.lan:8080/api/save
```

**Response:**
```json
{
  "status": "success",
  "message": "Configuration saved"
}
```

#### POST /api/restart

**Description:** Reboot device

**Example Request:**
```bash
curl -X POST http://allskyesp32.lan:8080/api/restart
```

**Response:**
```json
{
  "status": "success",
  "message": "Device rebooting..."
}
```

#### POST /api/factory_reset

**Description:** Clear all NVS settings and reboot to captive portal

**Example Request:**
```bash
curl -X POST http://allskyesp32.lan:8080/api/factory_reset
```

**Response:**
```json
{
  "status": "success",
  "message": "Factory reset complete, rebooting..."
}
```

**Warning:** This clears all configuration including WiFi credentials. Device will reboot to captive portal mode.

---

## Next Steps

- [Configuration Guide](03_configuration.md) - Detailed configuration options
- [OTA Updates](05_ota_updates.md) - Wireless firmware updates
- [Troubleshooting](06_troubleshooting.md) - Common issues and solutions
- [Hardware Guide](01_hardware.md) - Hardware specifications and setup
- [Installation Guide](02_installation.md) - Step-by-step setup
