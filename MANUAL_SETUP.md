# Manual Setup Guide

**When to use this:** Compiling from source, hardcoded WiFi credentials, advanced deployments

**Recommended for most users:** Flash pre-compiled binary and use captive portal (see [README](README.md))

---

## Compiling From Source

### Prerequisites

- **Arduino IDE** 1.8.19+ or **PlatformIO**
- **ESP32 Arduino Core** 3.3.4+
- **Required Libraries:** GFX Library for Arduino (1.6.3+), JPEGDEC (1.8.4+), PubSubClient (2.8.0+), ElegantOTA

### Arduino IDE Setup

<img src="images/ArduinoIDE.jpg" alt="Arduino IDE Configuration" width="500">

**Board Settings:**
- Board: ESP32-P4-Function-EV-Board
- Flash Size: 32MB
- Partition Scheme: 13MB app / 7MB data (32MB)
- PSRAM: **Enabled** (required)

### Compile & Upload

**Option 1 - PowerShell Script:**
```powershell
.\compile-and-upload.ps1 -ComPort COM3
```

**Option 2 - Arduino IDE:**
Click Upload button (→) or press Ctrl+U

**Option 3 - Export Binary Only:**
```powershell
.\compile-and-upload.ps1 -SkipUpload
```
Binary saved to Downloads folder for later flashing.

---

## Hardcoded WiFi Configuration

**When to use:** Factory provisioning, multiple devices with same credentials, closed deployments

### Edit WiFi Credentials

**File:** `config_storage.cpp` → `setDefaults()` function

```cpp
config.wifiProvisioned = true;           // Enable manual mode
config.wifiSSID = "YOUR_WIFI_SSID";      // Your network name
config.wifiPassword = "YOUR_PASSWORD";   // Your network password
```

### Optional: Pre-configure Settings

**MQTT** (same file):
```cpp
config.mqttServer = "192.168.1.250";
config.mqttPort = 1883;
config.mqttUser = "";
config.mqttPassword = "";
```

**Image Sources** (`config.cpp`):
```cpp
const char* DEFAULT_IMAGE_SOURCES[] = {
    "http://your-allsky-server.com/resized/image.jpg",
    "http://camera2.com/image.jpg",
    // Up to 10 sources
};
```

### Compile & Upload

After editing configuration, compile and upload using methods described in "Compiling From Source" section above.

### Verify Connection

Serial monitor (9600 baud) shows IP address. Access web UI at `http://[device-ip]:8080/`

---

## Important Notes

**Security Warning:** Don't commit credentials to version control. Use for:
- Automated factory provisioning
- Closed-source deployments
- Multiple devices with same network

**Normal users:** Use captive portal (default) - see [README](README.md)

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Won't connect | Verify SSID/password case-sensitive, check 2.4GHz network |
| No IP shown | Check router DHCP list, serial monitor, or `http://allskyesp32.lan:8080/` |
| Need to change WiFi | Use web interface or factory reset to trigger AP mode |

---

## Reset to Defaults

To return to captive portal mode:
1. Web interface → Factory Reset, OR
2. Serial command `F`, OR
3. `esptool.py erase_flash`
