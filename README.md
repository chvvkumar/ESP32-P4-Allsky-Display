# ESP32-P4 AllSky Display

An image display for [AllSky](https://github.com/AllskyTeam/allsky) cameras using ESP32-P4 with multi-image cycling, hardware acceleration, web configuration, and Home Assistant integration.

<img src="images/display.jpg" alt="Display in Action" width="600">

## 🎥 Demo

Check out the video below to see the display in action:

[![Demo Video](https://img.youtube.com/vi/pPAgbkPNvvY/0.jpg)](https://www.youtube.com/watch?v=pPAgbkPNvvY)

## Table of Contents

- [Build Status](#build-status)
- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Installation](#installation)
  - [Prerequisites](#prerequisites)
  - [Arduino IDE Setup](#arduino-ide-setup)
  - [Initial Setup](#initial-setup)
- [Configuration](#configuration)
  - [Web Interface](#web-interface)
  - [Home Assistant MQTT Discovery](#home-assistant-mqtt-discovery)
- [Image Optimization for AllSky](#image-optimization-for-allsky)
  - [Automated Image Resizing Script](#automated-image-resizing-script)
  - [Setup Options](#setup-options)
- [Web Configuration Interface](#web-configuration-interface)
  - [Configuration Pages](#configuration-pages)
  - [Home Assistant Integration](#home-assistant-integration)
  - [Touch Controls](#touch-controls)
  - [Serial Commands](#serial-commands)
- [Troubleshooting](#troubleshooting)
  - [Common Issues](#common-issues)
  - [Debug Information](#debug-information)
- [Contributing](#contributing)
- [License](#license)
- [Support](#support)

## 🚀 Build Status

[![Arduino Compilation Check](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/arduino-compile.yml/badge.svg)](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/arduino-compile.yml)
[![Create Release](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/release.yml/badge.svg)](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/release.yml)
[![Memory Usage Analysis](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/memory-analysis.yml/badge.svg)](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/actions/workflows/memory-analysis.yml)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/chvvkumar/ESP32-P4-Allsky-Display)](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/releases/latest)

![Flash Usage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/chvvkumar/ESP32-P4-Allsky-Display/main/.github/badges/flash.json)
![RAM Usage](https://img.shields.io/endpoint?url=https://raw.githubusercontent.com/chvvkumar/ESP32-P4-Allsky-Display/main/.github/badges/ram.json)

## 🌟 Overview

This project makes your ESP32-P4 touch display into an all-sky camera viewer with features like image cycling, per-image transformations, MQTT control, and seamless Home Assistant integration.

**Key Highlights:**
- Display images from multiple sources with automatic cycling
- Real-time image transformations (scale, offset, rotation)
- Complete web-based configuration interface
- Native Home Assistant integration via MQTT discovery
- Touch controls for quick navigation
- Hardware-accelerated image processing using ESP32-P4 PPA

## ✨ Features

- **Multi-source image cycling**: Display images from up to 10 different URLs with automatic cycling
- **Hardware-accelerated processing**: Fast image scaling and rotation using ESP32-P4 PPA
- **Per-image transformations**: Individual scale, offset, and rotation settings for each image
- **Web-based configuration**: Complete setup interface accessible via browser
- **Home Assistant integration**: Native MQTT discovery with full control and monitoring
- **Touch controls**: Single tap for next image, double tap to toggle modes

### Interactive Controls
- **Touch interface**: 
  - Single tap: Next image
  - Double tap: Toggle cycling/refresh modes
- **Serial commands**: Real-time manipulation via USB
- **Transform controls**: Scale (+/-), Move (WASD), Rotate (QE)

## 🛠️ Hardware Requirements

**Supported Displays:**
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm) (800x800)
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) (720x720)

**Specifications:**
- ESP32-P4 MCU with WiFi 6
- DSI touch display
- GT911 capacitive touch controller
- 16MB flash, PSRAM support required

**Optional 3D Printed Case:**

<img src="images/Case.jpg" alt="3D Printed Case" width="600">

[Download from Printables](https://www.printables.com/model/1352883-desk-stand-for-waveshare-esp32-p4-wifi6-touch-lcd)

## 📦 Installation

### Prerequisites

1. **Arduino IDE** (1.8.19 or later) or **PlatformIO**
2. **ESP32 Arduino Core** (3.3.4+)
3. **Required Libraries**:
   - GFX Library for Arduino (1.6.3+)
   - JPEGDEC (1.8.4+)
   - PubSubClient (2.8.0+)

### Arduino IDE Setup

<img src="images/ArduinoIDE.jpg" alt="Arduino IDE Configuration" width="600">

### Initial Setup

#### 1. Configure WiFi Credentials

**⚠️ IMPORTANT**: Edit WiFi settings **before** first upload.

Open [`config_storage.cpp`](config_storage.cpp) and modify:

````cpp
void ConfigStorage::setDefaults() {
    // *** EDIT THESE LINES ***
    config.wifiSSID = "YOUR_WIFI_SSID";        // Your network name
    config.wifiPassword = "YOUR_WIFI_PASSWORD"; // Your password
    
    // MQTT settings (optional, can configure via web later)
    config.mqttServer = "192.168.1.250";       // MQTT broker IP
    config.mqttPort = 1883;
    config.mqttUser = "";                       // Optional
    config.mqttPassword = "";                   // Optional
}
````

#### 2. Configure Default Image Source (Optional)

In [`config.cpp`](config.cpp), set your default image URLs:

```cpp
const char* DEFAULT_IMAGE_SOURCES[] = {
    "http://your-server.com/image1.jpg",
    "http://your-server.com/image2.jpg",
    // Add more as needed
};
```

#### 3. Compile and Upload

**Option A: Using Arduino IDE**
- Click the Upload button in Arduino IDE
- Monitor serial output for IP address and status

**Option B: Using the PowerShell Script (Windows)**

A convenient PowerShell script is provided for automated compilation and upload:

```powershell
# Default usage (COM3, 921600 baud)
.\compile-and-upload.ps1

# Custom COM port
.\compile-and-upload.ps1 -ComPort COM5

# Custom baud rate
.\compile-and-upload.ps1 -ComPort COM3 -BaudRate 115200

# Upload only (skip compilation)
.\compile-and-upload.ps1 -UploadOnly
```

**Requirements:**
- [Arduino CLI](https://arduino.github.io/arduino-cli/) must be installed
- Script auto-detects paths and tool versions
- Works from any directory with proper ESP32 board package installed

**Features:**
- Automatic path detection (no hardcoded user paths)
- Single-line upload progress display
- Memory usage analysis
- Colored output for easy monitoring
- Cross-machine compatible

#### 4. Access Web Interface

- Open a browser and go to `http://[device-ip]:8080/`
- Configure additional settings like MQTT and image sources

## ⚙️ Configuration

### Web Interface
- Access: `http://[device-ip]:8080/`
- Configure image sources, display settings, network, and MQTT

### Home Assistant MQTT Discovery
The device automatically integrates with Home Assistant when MQTT discovery is enabled:

**Entities Created:**
- **Light**: Brightness control (0-100%)
- **Switches**: Cycling enabled, Random order, Auto brightness
- **Numbers**: Cycle interval, Update interval, Per-image transforms (scale, offset, rotation)
- **Select**: Current image source selector
- **Buttons**: Reboot device, Next image, Reset transforms
- **Sensors**: Current image URL, Free heap, Free PSRAM, WiFi signal, Uptime, Image count

**Setup:**
1. Configure MQTT broker settings in web interface
2. Enable "Home Assistant Discovery" in MQTT settings
3. Set device name and topic preferences (optional)
4. Save and reconnect MQTT
5. Device appears automatically in Home Assistant

All device controls and sensors will be available in Home Assistant for dashboards, automations, and scripts.

## 📷 Image Optimization for AllSky

**⚠️ IMPORTANT**: The ESP32-P4 has limited resources. Large images (over 1MB or high resolution) can cause crashes or out-of-memory errors. It's highly recommended to resize images before serving them to the device.

### Automated Image Resizing Script

Use this script to automatically resize AllSky images to an optimal size for the display:

```bash
#!/bin/bash

# Configuration variables
INPUT_DIR="/home/pi/allsky/tmp"
OUTPUT_DIR="/home/pi/allsky/resized"
IMAGE_NAME="image.jpg"
RESIZE_DIMENSIONS="720x720"

# Create output directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Resize allsky image on demand
/usr/bin/mogrify -path "${OUTPUT_DIR}" -resize "${RESIZE_DIMENSIONS}" "${INPUT_DIR}/${IMAGE_NAME}" >/dev/null 2>&1
```

### Setup Options

#### Option 1: AllSky Script Module (Recommended)

1. Save the script as `/home/pi/allsky/scripts/resize_for_display.sh`
2. Make it executable: `chmod +x /home/pi/allsky/scripts/resize_for_display.sh`
3. In AllSky Web UI, go to **Module Manager → Daytime or Night time capture → Add the 'Allsky script' module to the selected modules → add script path to the module config**
4. Point your display to `http://your-allsky-server/resized/image.jpg`

#### Option 2: Cron Job

1. Save the script to `/home/pi/resize_allsky.sh`
2. Make it executable: `chmod +x /home/pi/resize_allsky.sh`
3. Edit crontab: `crontab -e`
4. Add one of these lines based on your preference:

```bash
# Run every minute
* * * * * /home/pi/resize_allsky.sh

# Run every 5 minutes
*/5 * * * * /home/pi/resize_allsky.sh

# Run every 10 minutes
*/10 * * * * /home/pi/resize_allsky.sh
```

5. Save and exit
6. Point your display to `http://your-allsky-server/current/resized/image.jpg`

**Note**: Adjust `RESIZE_DIMENSIONS` based on your display model (720x720 for 4C, 800x800 for 3.4C).

## 🖥️ Web Configuration Interface

The device provides a comprehensive web-based configuration interface accessible at `http://[device-ip]:8080/`. All settings can be configured through an intuitive UI without needing to recompile the firmware.

### Configuration Pages

<img src="images/2025-12-03_10-55-20.png" alt="Web Configuration Main Page">

<img src="images/2025-12-03_10-55-31.png" alt="Image Sources Configuration">

<img src="images/2025-12-03_10-55-35.png" alt="Display Settings">

<img src="images/2025-12-03_10-55-38.png" alt="Network Configuration">

<img src="images/2025-12-03_10-55-42.png" alt="MQTT Configuration">

<img src="images/2025-12-03_10-55-48.png" alt="System Information">

### Home Assistant Integration

<img src="images/Home Assistant.jpg" alt="Home Assistant Integration">

### Touch Controls
```
Single Tap  : Next image (cycling mode)
Double Tap  : Toggle cycling/refresh modes
```

### Serial Commands
```
+/-   : Scale image        R     : Reset transforms
W/S   : Move up/down       L/K   : Brightness
A/D   : Move left/right    B     : Reboot
Q/E   : Rotate CCW/CW      M/I/P : System info
```

## 🔌 API Reference

The device exposes a REST API for programmatic control at `http://[device-ip]:8080/api/`. All API endpoints accept POST requests with form-encoded or JSON data.

### Configuration Management

#### Save Configuration
```bash
POST /api/save
```
**Parameters:**
- `wifi_ssid` - WiFi network name
- `wifi_password` - WiFi password
- `mqtt_server` - MQTT broker IP/hostname
- `mqtt_port` - MQTT broker port (default: 1883)
- `mqtt_user` - MQTT username (optional)
- `mqtt_password` - MQTT password (optional)
- `mqtt_client_id` - MQTT client identifier
- `ha_device_name` - Home Assistant device name
- `ha_discovery_prefix` - HA discovery prefix (default: homeassistant)
- `ha_state_topic` - HA state topic
- `ha_sensor_update_interval` - Sensor update interval in seconds
- `image_url` - Legacy single image URL
- `default_brightness` - Display brightness (0-255)
- `default_scale_x` - X-axis scale factor
- `default_scale_y` - Y-axis scale factor
- `default_offset_x` - X-axis offset in pixels
- `default_offset_y` - Y-axis offset in pixels
- `default_rotation` - Rotation angle in degrees
- `cycle_interval` - Time between images in seconds
- `update_interval` - Image refresh interval in minutes

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/save \
  -d "default_brightness=200" \
  -d "cycle_interval=30"
```

### Image Source Management

#### Add Image Source
```bash
POST /api/add-source
```
**Parameters:**
- `url` - Image URL to add

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/add-source \
  -d "url=https://example.com/image.jpg"
```

#### Remove Image Source
```bash
POST /api/remove-source
```
**Parameters:**
- `index` - Index of image to remove (0-based)

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/remove-source \
  -d "index=2"
```

#### Update Image Source
```bash
POST /api/update-source
```
**Parameters:**
- `index` - Index of image to update (0-based)
- `url` - New image URL

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/update-source \
  -d "index=0" \
  -d "url=https://example.com/new-image.jpg"
```

#### Clear All Image Sources
```bash
POST /api/clear-sources
```
**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/clear-sources
```

### Image Navigation

#### Next Image
```bash
POST /api/next-image
```
Manually advance to the next image in the cycle.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/next-image
```

#### Previous Image
```bash
POST /api/previous-image
```
Go back to the previous image.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/previous-image
```

### Image Transformations

#### Update Image Transform
```bash
POST /api/update-transform
```
**Parameters:**
- `index` - Image index (0-based)
- `scale_x` - X-axis scale factor
- `scale_y` - Y-axis scale factor
- `offset_x` - X-axis offset in pixels
- `offset_y` - Y-axis offset in pixels
- `rotation` - Rotation angle in degrees

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/update-transform \
  -d "index=0" \
  -d "scale_x=1.2" \
  -d "scale_y=1.2" \
  -d "offset_x=50" \
  -d "offset_y=-30" \
  -d "rotation=0"
```

#### Copy Defaults to Image
```bash
POST /api/copy-defaults
```
Copy default transformation settings to a specific image.

**Parameters:**
- `index` - Target image index (0-based)

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/copy-defaults \
  -d "index=1"
```

#### Apply Transform
```bash
POST /api/apply-transform
```
Immediately apply transformation changes and refresh the display.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/apply-transform
```

### System Control

#### Restart Device
```bash
POST /api/restart
```
Reboot the ESP32 device.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/restart
```

#### Factory Reset
```bash
POST /api/factory-reset
```
Reset all settings to factory defaults and reboot.

**Example:**
```bash
curl -X POST http://192.168.1.100:8080/api/factory-reset
```

### Device Information

#### Get Device Info
```bash
GET /api/device-info
```
Retrieve comprehensive device information including hardware specifications, memory usage, flash metrics, network status, and system health.

**Response includes:**
- **device** - Chip model, revision, CPU cores/frequency, SDK version
- **flash** - Size, speed, sketch size, usage percentage, free space, MD5 hash
- **memory** - Heap and PSRAM size, free/used amounts, minimum free values
- **network** - WiFi status, SSID, IP address, MAC, RSSI, gateway, DNS
- **mqtt** - Connection status, server, port, client ID, HA discovery status
- **display** - Brightness level, auto mode status, resolution
- **imageCycling** - Enabled status, current index, total sources, intervals
- **system** - Uptime, health status, temperature

**Example:**
```bash
curl http://192.168.1.100:8080/api/device-info
```

**Sample Response:**
```json
{
  "device": {
    "name": "ESP32-P4 AllSky Display",
    "chipModel": "ESP32-P4",
    "chipRevision": 0,
    "cpuCores": 2,
    "cpuFreqMHz": 400,
    "sdkVersion": "5.5",
    "arduinoVersion": "10812"
  },
  "flash": {
    "size": 16777216,
    "speed": 80000000,
    "sketchSize": 1316020,
    "sketchUsedPercent": 7,
    "freeSketchSpace": 15461196,
    "sketchMD5": "abc123..."
  },
  "memory": {
    "heapSize": 327680,
    "freeHeap": 273580,
    "heapUsedPercent": 16,
    "psramSize": 8388608,
    "freePsram": 8200000
  },
  "network": {
    "connected": true,
    "ssid": "YourNetwork",
    "ipAddress": "192.168.1.100",
    "rssi": -45
  }
}
```

### API Response Format

All API endpoints return JSON responses:

**Success Response:**
```json
{
  "status": "success",
  "message": "Operation completed successfully"
}
```

**Error Response:**
```json
{
  "status": "error",
  "message": "Error description"
}
```

### Integration Examples

#### Python Script
```python
import requests
import json

# Device IP
device_ip = "192.168.1.100"
base_url = f"http://{device_ip}:8080/api"

# Get device information
info = requests.get(f"{base_url}/device-info").json()
print(f"Device: {info['device']['chipModel']}")
print(f"Flash used: {info['flash']['sketchUsedPercent']}%")
print(f"Free heap: {info['memory']['freeHeap']:,} bytes")
print(f"Free PSRAM: {info['memory']['freePsram']:,} bytes")
print(f"Uptime: {info['system']['uptime'] / 1000:.0f} seconds")

# Set brightness
requests.post(f"{base_url}/save", data={"default_brightness": 200})

# Add new image source
requests.post(f"{base_url}/add-source", 
              data={"url": "https://example.com/weather.jpg"})

# Navigate to next image
requests.post(f"{base_url}/next-image")

# Update transformation for image 0
requests.post(f"{base_url}/update-transform", data={
    "index": 0,
    "scale_x": 1.5,
    "scale_y": 1.5,
    "offset_x": 0,
    "offset_y": 0,
    "rotation": 0
})
```

#### Home Assistant Automation
```yaml
automation:
  - alias: "Update Display at Sunset"
    trigger:
      - platform: sun
        event: sunset
    action:
      - service: rest_command.display_next_image
        
rest_command:
  display_next_image:
    url: "http://192.168.1.100:8080/api/next-image"
    method: POST
    
  display_set_brightness:
    url: "http://192.168.1.100:8080/api/save"
    method: POST
    payload: "default_brightness={{ brightness }}"
```

#### Node-RED Flow
```json
[
    {
        "type": "http request",
        "method": "POST",
        "url": "http://192.168.1.100:8080/api/next-image",
        "name": "Next Image"
    }
]
```

## 🐛 Troubleshooting

### Common Issues
1. **Compilation**: Enable PSRAM, check ESP32 core version
2. **Memory**: Monitor PSRAM usage in serial output
3. **Network**: Verify WiFi credentials and image URL access
4. **Touch**: Check GT911 I2C connections and debug output

### Debug Information
- Serial output for comprehensive debugging
- Web interface for real-time system status
- Memory and network monitoring

## Contributing

1. Fork the repository
2. Create a feature branch
3. Test thoroughly
4. Submit a pull request

## License

Open source project. Check license file for details.

## Support

- Create GitHub issues for bugs/features
- Check troubleshooting section
- Monitor serial output for debugging
