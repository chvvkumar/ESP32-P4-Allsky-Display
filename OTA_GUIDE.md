# Over-The-Air (OTA) Update Guide

## Overview

The ESP32-P4 AllSky Display supports two methods of wireless firmware updates:

1. **ElegantOTA** - Web-based OTA with professional UI (Recommended for most users)
2. **ArduinoOTA** - IDE-integrated OTA (Recommended for developers)

Both methods use **safe A/B partitioning** with automatic rollback protection, ensuring your device remains functional even if an update fails.

## Table of Contents

- [Quick Start](#quick-start)
- [Method 1: ElegantOTA (Web Interface)](#method-1-elegantota-web-interface)
- [Method 2: ArduinoOTA (Arduino IDE)](#method-2-arduinoota-arduino-ide)
- [Partition Scheme](#partition-scheme)
- [Update Process](#update-process)
- [Troubleshooting](#troubleshooting)
- [Security Considerations](#security-considerations)
- [Developer Reference](#developer-reference)

---

## Quick Start

### For End Users (ElegantOTA)

1. Compile firmware → get `.bin` file
2. Open `http://[device-ip]:8080/update` in browser
3. Upload `.bin` file
4. Wait for automatic reboot (~30-60 seconds)

### For Developers (ArduinoOTA)

1. Ensure device is on same WiFi network
2. Select OTA port in Arduino IDE: `Tools → Port → esp32-allsky-display at [IP]`
3. Click Upload button
4. Monitor progress on device screen

---

## Method 1: ElegantOTA (Web Interface)

#### 1. Prepare the Firmware

**Option A: Using PowerShell Script (Recommended)**

```powershell
.\compile-and-upload.ps1
```

The script will:
- Compile the firmware
- Save `.bin` file to `./build/` directory
- Show the file location

**Option B: Using Arduino IDE**

1. Open the project in Arduino IDE
2. Go to `Sketch → Export Compiled Binary` (or press `Ctrl+Alt+S`)
3. Wait for compilation to complete
4. The `.bin` file will be saved in the sketch folder
5. Look for: `ESP32-P4-Allsky-Display.ino.esp32p4.bin`

**Option C: Manual Arduino CLI**

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled --output-dir ./build
```

#### 2. Access ElegantOTA Interface

1. **Find Device IP Address**:
   - Check your display screen (shows IP on boot)
   - Check your router's DHCP client list
   - Use network scanner app

2. **Open ElegantOTA**:
   - Navigate to: `http://[device-ip]:8080/update`
   - You should see the ElegantOTA interface

   <img src="images/elegantota-interface.png" alt="ElegantOTA Interface" width="600">

#### 3. Upload Firmware

1. **Select Firmware Type**: Choose "Firmware" (default)
   - **Firmware**: Main application binary (.bin file)
   - **Filesystem**: SPIFFS/LittleFS image (not used in this project)

2. **Choose File**:
   - Click "Choose File" button
   - Or drag and drop the `.bin` file onto the interface
   - Select your compiled binary (e.g., `ESP32-P4-Allsky-Display.ino.esp32p4.bin`)

3. **Start Update**:
   - Click "Update" button
   - Progress bar will show upload progress
   - Device display will show: "OTA Update starting..." and progress percentage

4. **Wait for Completion**:
   - Upload typically takes 30-60 seconds
   - Device will show "OTA Progress: 10%, 20%, 30%..." on screen
   - After upload completes, verification runs automatically
   - Device displays "OTA Complete! Rebooting..."
   - Device automatically reboots with new firmware

5. **Verify Success**:
   - Device should boot normally
   - Check serial monitor for boot messages
   - Verify new version in web interface: `http://[device-ip]:8080/`

---

## Method 2: ArduinoOTA (Arduino IDE)

### Prerequisites

1. **Same Network**: Device must be on same WiFi network as your computer
2. **Arduino IDE**: Version 1.8.19 or later
3. **ESP32 Arduino Core**: Version 3.3.4 or later with ArduinoOTA support
4. **Port Selection**: Device must appear in IDE's port list

### Step-by-Step Instructions

#### 1. Initial Setup (One-Time)

1. **Ensure Device is Connected to WiFi**:
   - Use captive portal or web interface to configure WiFi
   - Device must be on same network as your development machine

2. **Open Arduino IDE**:
   - Load the ESP32-P4-Allsky-Display project
   - Wait for IDE to fully initialize

#### 2. Select OTA Port

1. **Find Device in Port List**:
   - Go to `Tools → Port`
   - Look for: `esp32-allsky-display at [IP-address]`
   - Example: `esp32-allsky-display at 192.168.1.100`

   <img src="images/arduino-ota-port.png" alt="Arduino OTA Port Selection" width="400">

2. **Select the Network Port**:
   - Click on the network port entry
   - Port icon shows a WiFi symbol (📶) instead of USB symbol

**Note**: If device doesn't appear:
- Verify device is connected to WiFi (check display for IP)
- Ensure mDNS is working on your network
- Check firewall settings (allow port 3232)
- Wait 30-60 seconds for device to announce itself

#### 3. Upload Firmware

1. **Click Upload Button** (➡️):
   - Standard Arduino IDE upload button
   - IDE automatically uses OTA method when network port is selected

2. **Monitor Progress**:
   - **Arduino IDE**: Shows "Uploading..." in status bar
   - **Serial Monitor**: Prints upload progress (if connected)
   - **Device Screen**: Displays real-time progress:
     ```
     OTA Update starting...
     Firmware update...
     OTA Progress: 10%
     OTA Progress: 20%
     ...
     OTA Progress: 100%
     OTA Complete! Rebooting...
     ```

3. **Wait for Completion**:
   - Upload typically takes 30-60 seconds
   - Device will reboot automatically
   - Arduino IDE shows "Upload complete"

4. **Verify Success**:
   - Device boots with new firmware
   - Serial monitor shows startup messages
   - Device resumes normal operation

### ArduinoOTA Configuration

ArduinoOTA is configured in `network_manager.cpp` with the following settings:

```cpp
Hostname: esp32-allsky-display
Port: 3232 (default)
Password: None (disabled by default)
```

**To enable OTA password protection**, uncomment this line in `network_manager.cpp`:

```cpp
ArduinoOTA.setPassword("your-password-here");
```

Then recompile and upload via USB once.

### Development Workflow

For rapid development, use ArduinoOTA:

1. **Initial Upload**: USB cable (first time only)
2. **Code Changes**: Edit source code
3. **OTA Upload**: Click Upload (device stays mounted on wall/desk)
4. **Test**: Verify changes on device
5. **Repeat**: No cable reconnection needed

This workflow is ideal when the device is mounted in a permanent location.

---

## Partition Scheme

The device uses a **13MB app / 7MB data** partition scheme with OTA support:

```
Flash: 32MB total
├── Bootloader (~256KB)
├── Partition Table (~4KB)
├── OTA_0 (App partition 0): 13MB
├── OTA_1 (App partition 1): 13MB
├── NVS (Config storage): ~256KB
└── SPIFFS/Data: ~7MB (reserved for future use)
```

### How A/B Partitioning Works

1. **Current Firmware**: Runs from OTA_0 partition
2. **New Upload**: Written to OTA_1 partition (inactive)
3. **Verification**: Bootloader validates new firmware
4. **Switch**: Boot flag points to OTA_1
5. **First Boot**: New firmware runs from OTA_1
6. **Validation**: If firmware boots successfully, OTA_1 becomes active
7. **Rollback**: If boot fails, bootloader automatically reverts to OTA_0

This ensures your device **never gets bricked** from a bad OTA update.

### Partition Configuration

Defined in `partitions.csv`:

```csv
# Name,   Type, SubType, Offset,  Size,   Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0xD00000,
app1,     app,  ota_1,   0xD10000,0xD00000,
spiffs,   data, spiffs,  0x1A10000,0x700000,
```

**To use this partition scheme in Arduino IDE**:

`Tools → Partition Scheme → 13MB app / 7MB data (32MB)`

Or specify in FQBN:

```
esp32:esp32:esp32p4:PartitionScheme=app13M_data7M_32MB
```

---

## Update Process

### What Happens During OTA Update

#### Phase 1: Preparation (1-2 seconds)
- Display pauses to prevent memory conflicts
- Watchdog timer is extended to prevent timeout
- Current configuration is preserved in NVS
- Device shows: "OTA Update starting..."

#### Phase 2: Download/Upload (30-60 seconds)
- Firmware is downloaded to inactive OTA partition
- Progress updates every 10%:
  - Device screen: "OTA Progress: 20%"
  - Serial output: Real-time progress
- Watchdog is reset on each progress update

#### Phase 3: Verification (5-10 seconds)
- Bootloader validates firmware checksum
- Partition integrity check
- Boot flag is set to new partition

#### Phase 4: Reboot & Validation (10-15 seconds)
- Device reboots automatically
- Bootloader loads new firmware from inactive partition
- New firmware initializes all systems
- If successful, new partition becomes active
- If failed, automatic rollback to previous firmware

#### Phase 5: Resume Operation
- Display resumes normal operation
- Configuration is loaded from NVS (preserved)
- Device reconnects to WiFi/MQTT
- Image display continues from last state

### Safety Features

1. **Watchdog Protection**:
   - Extended timeout during update (prevents premature reboot)
   - Reset on each progress update

2. **Display Pause**:
   - Prevents memory conflicts during upload
   - Eliminates screen glitches

3. **Automatic Rollback**:
   - Boot validation on first run
   - Reverts to previous firmware if new firmware fails
   - No user intervention required

4. **Configuration Preservation**:
   - WiFi credentials retained
   - MQTT settings retained
   - Image sources and transforms retained
   - Display settings retained

---

## Troubleshooting

### ElegantOTA Issues

#### "Upload Failed" Error

**Possible Causes:**
- Insufficient memory during upload
- Incorrect partition scheme
- Corrupted binary file
- Network interruption

**Solutions:**
1. **Verify Partition Scheme**:
   - Must use: `13MB app / 7MB data (32MB)`
   - Recompile with correct partition scheme

2. **Check Binary File**:
   - Ensure file is complete (not truncated)
   - Verify file size (~1-3MB typical)
   - Recompile if suspicious

3. **Free Memory**:
   - Reboot device before OTA update
   - Close other applications on device

4. **Stable Network**:
   - Use wired connection for development machine
   - Ensure strong WiFi signal to device
   - Avoid network-intensive tasks during upload

#### "Cannot Access OTA Interface"

**Solutions:**
1. **Verify IP Address**:
   - Check display screen for correct IP
   - Ping device: `ping [device-ip]`

2. **Check Port 8080**:
   - Ensure firewall allows port 8080
   - Try: `http://[device-ip]:8080/update`

3. **Restart Web Server**:
   - Reboot device
   - Check serial output for web server startup

#### Progress Stuck at 100%

**Cause**: Normal verification phase

**Solution**: Wait 10-15 seconds for verification and reboot

### ArduinoOTA Issues

#### Device Not Appearing in Port List

**Solutions:**
1. **Check WiFi Connection**:
   - Verify device is connected to WiFi
   - Check display for IP address
   - Ensure same network as development machine

2. **mDNS Issues**:
   - Restart Arduino IDE
   - Wait 60 seconds for device announcement
   - Check router mDNS/Bonjour settings

3. **Firewall Settings**:
   - Allow port 3232 (UDP and TCP)
   - Allow mDNS port 5353 (UDP)
   - Temporarily disable firewall to test

4. **Network Configuration**:
   - Avoid VLANs between device and dev machine
   - Check for AP isolation on WiFi
   - Use same subnet

#### "Upload Error: Connection Failed"

**Solutions:**
1. **Verify Port Selection**:
   - Ensure network port is selected, not USB
   - Look for WiFi symbol (📶) next to port name

2. **Device Reachability**:
   - Ping device IP: `ping [device-ip]`
   - Test port: `telnet [device-ip] 3232`

3. **Network Stability**:
   - Strong WiFi signal to device
   - Stable connection during upload

4. **Restart OTA Service**:
   - Reboot device
   - Wait for WiFi reconnection

#### Upload Timeout

**Cause**: Network latency or watchdog timeout

**Solutions:**
1. **Improve Signal**:
   - Move device closer to router
   - Reduce wireless interference

2. **Check Watchdog**:
   - Watchdog is automatically extended during OTA
   - If persistent, increase `IMAGE_PROCESS_TIMEOUT` in config.h

### General OTA Issues

#### Device Reboots During Upload

**Cause**: Watchdog timeout or power issue

**Solutions:**
1. **Power Supply**:
   - Use quality USB power adapter (2A minimum)
   - Check USB cable quality
   - Avoid long/thin USB cables

2. **Watchdog**:
   - Code automatically resets watchdog
   - If issue persists, check serial output for errors

#### OTA Update Succeeds but Device Won't Boot

**Cause**: Firmware incompatibility or configuration mismatch

**Solutions:**
1. **Automatic Rollback**:
   - Device should automatically roll back to previous firmware
   - Wait 30 seconds for rollback to complete

2. **Manual Recovery** (if rollback fails):
   - Connect via USB
   - Upload firmware via USB cable
   - Check serial monitor for boot errors

3. **Factory Reset** (last resort):
   - Hold reset button while powering on
   - Or use serial command `F` for factory reset
   - Reconfigure via captive portal

#### Configuration Lost After Update

**Cause**: NVS corruption (rare)

**Prevention**:
- Configuration is automatically preserved in NVS
- Survives firmware updates

**Recovery**:
1. Reconfigure via web interface
2. Check serial output for NVS errors
3. Factory reset if NVS is corrupted

---

## Security Considerations

### Default Configuration

- **ElegantOTA**: No authentication (open access)
- **ArduinoOTA**: No password (open access)

This is suitable for **home networks** with trusted users.

### Securing OTA Updates

#### Option 1: Enable ArduinoOTA Password

Edit `network_manager.cpp`:

```cpp
// Uncomment this line:
ArduinoOTA.setPassword("your-strong-password");
```

Then recompile and upload once via USB.

**Arduino IDE will prompt for password** on subsequent OTA uploads.

#### Option 2: Network Isolation

- Place device on isolated IoT VLAN
- Restrict access via firewall rules
- Allow OTA only from development machine

#### Option 3: Disable OTA (Production)

Comment out OTA initialization in `network_manager.cpp`:

```cpp
// ArduinoOTA.begin();
```

And in `web_config.cpp`:

```cpp
// ElegantOTA.begin(server);
```

### Best Practices

1. **Home Network**: Default settings are usually sufficient
2. **Public Network**: Always enable password protection
3. **Production Deployment**: Use network isolation or disable OTA
4. **Development**: Use ArduinoOTA for convenience

### Firewall Ports

If using firewall, allow these ports:

- **Port 8080** (TCP): Web interface + ElegantOTA
- **Port 3232** (TCP/UDP): ArduinoOTA
- **Port 5353** (UDP): mDNS discovery (optional but recommended)

---

## Developer Reference

### OTA Manager API

The `OTAManager` class provides a unified interface for OTA progress tracking:

```cpp
// Initialize OTA manager
otaManager.begin();

// Set status
otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Starting update...");

// Update progress (0-100%)
otaManager.setProgress(50);

// Display progress on screen
otaManager.displayProgress("Firmware", 75);

// Check status
OTAUpdateStatus status = otaManager.getStatus();
uint8_t progress = otaManager.getProgress();

// Reset after update
otaManager.reset();
```

### OTA Status Enum

```cpp
enum OTAUpdateStatus {
    OTA_UPDATE_IDLE,           // No update in progress
    OTA_UPDATE_IN_PROGRESS,    // Update is running
    OTA_UPDATE_SUCCESS,        // Update completed successfully
    OTA_UPDATE_FAILED          // Update failed
};
```

### Integration Points

#### ElegantOTA Callbacks (web_config.cpp)

```cpp
ElegantOTA.onStart([]() {
    displayManager.debugPrint("OTA Update starting...", COLOR_YELLOW);
    displayManager.pauseDisplay();
    systemMonitor.forceResetWatchdog();
});

ElegantOTA.onProgress([](size_t current, size_t final) {
    systemMonitor.forceResetWatchdog();
    uint8_t percent = (current * 100) / final;
    otaManager.displayProgress("OTA", percent);
});

ElegantOTA.onEnd([](bool success) {
    displayManager.resumeDisplay();
    if (success) {
        displayManager.debugPrint("OTA Complete! Rebooting...", COLOR_GREEN);
    }
});
```

#### ArduinoOTA Callbacks (network_manager.cpp)

```cpp
ArduinoOTA.onStart([]() {
    otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "OTA Update starting...");
    displayManager.pauseDisplay();
});

ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    uint8_t percent = (progress * 100) / total;
    otaManager.setProgress(percent);
    otaManager.displayProgress("Firmware", percent);
    systemMonitor.forceResetWatchdog();
});

ArduinoOTA.onEnd([]() {
    otaManager.setStatus(OTA_UPDATE_SUCCESS, "OTA Complete! Rebooting...");
    displayManager.resumeDisplay();
});

ArduinoOTA.onError([](ota_error_t error) {
    otaManager.setStatus(OTA_UPDATE_FAILED, "OTA Update Failed");
    displayManager.resumeDisplay();
});
```

### Update Main Loop

Both OTA methods require periodic handling in main loop:

```cpp
void loop() {
    // Handle ArduinoOTA
    wifiManager.handleOTA();  // Calls ArduinoOTA.handle()
    
    // Handle ElegantOTA
    webConfig.handleClient();  // Calls ElegantOTA.loop()
}
```

### Custom OTA Handler

To implement custom OTA logic:

```cpp
#include "ota_manager.h"

void myCustomOTA() {
    // Start update
    otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Custom OTA starting...");
    displayManager.pauseDisplay();
    
    // Update progress during download
    for (int i = 0; i <= 100; i += 10) {
        otaManager.setProgress(i);
        otaManager.displayProgress("Custom", i);
        systemMonitor.forceResetWatchdog();
        delay(1000);
    }
    
    // Complete
    otaManager.setStatus(OTA_UPDATE_SUCCESS, "Custom OTA complete!");
    displayManager.resumeDisplay();
    
    // Reboot
    ESP.restart();
}
```

---

## Additional Resources

- [ElegantOTA Documentation](https://github.com/ayushsharma82/ElegantOTA)
- [ArduinoOTA Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ota.html)
- [ESP32 OTA Updates Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [ESP32 Partition Tables](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)

---

## Changelog

### Version History

- **v1.0** - Initial OTA support with ElegantOTA and ArduinoOTA
- **v1.1** - Added A/B partition scheme with automatic rollback
- **v1.2** - Enhanced progress display and watchdog protection
- **v1.3** - Improved error handling and recovery mechanisms

---

## Support

If you encounter issues with OTA updates:

1. Check this troubleshooting guide
2. Review serial monitor output during update
3. Verify partition scheme and binary file
4. Create GitHub issue with:
   - Update method used (ElegantOTA or ArduinoOTA)
   - Error messages from serial monitor
   - Network configuration
   - Steps to reproduce

---

## Summary

| Feature | ElegantOTA | ArduinoOTA |
|---------|------------|------------|
| **Interface** | Web browser | Arduino IDE |
| **Setup Complexity** | None | Minimal |
| **Best For** | End users | Developers |
| **Progress Display** | ✅ Yes | ✅ Yes |
| **Automatic Rollback** | ✅ Yes | ✅ Yes |
| **Password Protection** | ❌ Not built-in | ✅ Optional |
| **Mobile Friendly** | ✅ Yes | ❌ No |
| **One-Click Upload** | ❌ Need .bin file | ✅ Yes |

**Recommendation**: Use **ElegantOTA** for general updates, **ArduinoOTA** for development cycles.
