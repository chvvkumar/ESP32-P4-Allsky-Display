# ESP32-P4 AllSky Display

An image display for [AllSky](https://github.com/AllskyTeam/allsky) cameras using ESP32-P4 with multi-image cycling, hardware acceleration, web configuration, and Home Assistant integration.

<img src="images/display.jpg" alt="Display in Action" width="600">

## 🎥 Demo

Check out the video below to see the display in action:

[![Demo Video](https://img.youtube.com/vi/pPAgbkPNvvY/0.jpg)](https://www.youtube.com/watch?v=pPAgbkPNvvY)

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Home Assistant Integration](#home-assistant-integration)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)

## 🌟 Overview

This project transforms your ESP32-P4 touch display into a powerful all-sky camera viewer with advanced features like image cycling, per-image transformations, MQTT control, and seamless Home Assistant integration.

**Key Highlights:**
- Display images from multiple sources with automatic cycling
- Real-time image transformations (scale, offset, rotation)
- Complete web-based configuration interface
- Native Home Assistant integration via MQTT discovery
- Touch controls for quick navigation
- Hardware-accelerated image processing using ESP32-P4 PPA

## ✨ Features

### Image Display & Processing
- **Multi-source support**: Cycle through up to 10 image URLs
- **Hardware acceleration**: ESP32-P4 PPA for fast scaling and rotation
- **Per-image transforms**: Individual scale, offset, and rotation settings
- **Smart caching**: Reduces bandwidth with HTTP cache headers
- **Auto-refresh**: Configurable update intervals (1-1440 minutes)
- **JPEG support**: Efficient decoding with JPEGDEC library

### Web Configuration Interface
- **Modern UI**: Responsive design with real-time status updates
- **Network settings**: WiFi configuration without code changes
- **MQTT setup**: Easy broker configuration
- **Image management**: Add/remove/reorder sources via web interface
- **Display controls**: Brightness, timing, and transform settings
- **System monitoring**: Heap, PSRAM, WiFi signal, and uptime

### Home Assistant Integration
- **MQTT Discovery**: Automatic entity creation
- **Full control**: All settings accessible from HA
- **Per-image entities**: Scale, offset, rotation for each image
- **Live sensors**: Memory, WiFi, uptime monitoring
- **Buttons & switches**: Reboot, next image, cycling controls
- **Select entity**: Choose active image source

### Interactive Controls
- **Touch interface**: 
  - Single tap: Next image
  - Double tap: Toggle cycling/refresh modes
- **Serial commands**: Real-time manipulation via USB
- **Transform controls**: Scale (+/-), Move (WASD), Rotate (QE)

### System Features
- **WiFi manager**: Simple credential setup
- **Watchdog protection**: Prevents system freezes
- **Memory monitoring**: Automatic health checks
- **Error recovery**: Auto-reconnection and retry logic
- **OTA updates**: Ready for over-the-air firmware updates

## 🛠️ Hardware Requirements

**Supported Displays:**
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-3.4C](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm) (800x800)
- [Waveshare ESP32-P4-WIFI6-Touch-LCD-4C](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C) (720x720)

**Specifications:**
- ESP32-P4 MCU with WiFi 6
- DSI touch display
- GT911 capacitive touch controller
- 16MB flash, PSRAM support required

**Optional:**
- 3D printed case: [Download from Printables](https://www.printables.com/model/1352883-desk-stand-for-waveshare-esp32-p4-wifi6-touch-lcd)

## 📦 Installation

### Prerequisites

1. **Arduino IDE** (1.8.19 or later)
2. **Arduino CLI**
3. **ESP32 Arduino Core** (3.3.4)
   - Additional URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. **Required Libraries**:
   - GFX Library for Arduino (1.6.3)
   - JPEGDEC (1.8.4)
   - bb_spi_lcd (2.9.7)
   - PubSubClient (2.8.0)
5. **ESP32 Built-in Libraries** (v3.3.4):
   - Preferences, WebServer, FS, Networking, WiFi, SPI, Wire, HTTPClient, NetworkClientSecure, Hash

### Installation via Arduino CLI

```bash
# Update core index
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Install ESP32 core
arduino-cli core install esp32:esp32@3.3.4 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Install libraries
arduino-cli lib install "GFX Library for Arduino@1.6.3"
arduino-cli lib install "JPEGDEC@1.8.4"
arduino-cli lib install "PubSubClient@2.8.0"

# Patch GFX Library for ESP32-P4 compatibility
sed -i 's/.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,/.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, \/\/MIPI_DSI_PHY_CLK_SRC_DEFAULT,/' ~/Arduino/libraries/GFX_Library_for_Arduino/src/databus/Arduino_ESP32DSIPanel.cpp
```

### Build Configuration

**FQBN**: `esp32:esp32:esp32p4`

**Build Properties**:
- Flash Size: 32MB
- PSRAM Type: opi
- Partitions: huge_app
- Extra Flags: `-DBOARD_HAS_PSRAM`

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

#### 3. Select Correct Board in Arduino IDE

- `Tools > Board` → `ESP32P4 Dev Module`
- `Tools > Flash Size` → `16MB (128Mb)`
- `Tools > Partition Scheme` → `16M Flash (3MB APP/9.9MB FATFS)`

#### 4. Compile and Upload

- Click the Upload button in Arduino IDE
- Monitor serial output for IP address and status

### 5. Access Web Interface

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

