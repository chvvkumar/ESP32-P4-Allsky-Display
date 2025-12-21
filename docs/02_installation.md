# Installation Guide

## Table of Contents
- [Quick Start (Pre-compiled Binary)](#quick-start-pre-compiled-binary)
- [Compiling From Source](#compiling-from-source)
- [First Boot & WiFi Setup](#first-boot--wifi-setup)
- [Hardcoded WiFi Configuration](#hardcoded-wifi-configuration-advanced)
- [Troubleshooting Installation](#troubleshooting-installation)

---

## Quick Start (Pre-compiled Binary)

**Recommended for:** Most users, quickest setup, no development environment needed

### Step 1: Download Firmware

**✨ Display Compatibility Notice (v-snd-0.61+):**
- Pre-compiled `.bin` files work for **both 3.4" (800×800) and 4.0" (720×720)** displays
- **4.0" display users:** Flash the standard binary, then select "4.0" 720×720" in Web UI System settings
- Display type can be changed at runtime without recompilation (see [First Boot Configuration](#first-boot--wifi-setup))
- Default: 3.4" display on first boot

**Download Latest Release:**
1. Go to [Releases](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/releases/latest)
2. Download **`ESP32-P4-Allsky-Display-Factory.bin`** (universal for both displays)

⚠️ **Important:** For initial USB installation, use the **`-Factory.bin`** file. The **`-OTA.bin`** file is for wireless updates only and should NOT be flashed to address 0x0.

### Step 2: Flash Firmware

#### Option A: Using esptool (Command Line)

**Windows:**
```powershell
esptool.py --chip esp32p4 --port COM3 --baud 921600 write_flash 0x0 ESP32-P4-Allsky-Display-Factory.bin
```

**macOS/Linux:**
```bash
esptool.py --chip esp32p4 --port /dev/ttyUSB0 --baud 921600 write_flash 0x0 ESP32-P4-Allsky-Display-Factory.bin
```

**Find your port:**
- **Windows:** Device Manager → Ports (COM & LPT) → Look for USB-SERIAL CH340
- **macOS:** `ls /dev/tty.*` → Look for `/dev/tty.usbserial-*`
- **Linux:** `ls /dev/ttyUSB*` → Usually `/dev/ttyUSB0`

#### Option B: Using ESP Flash Download Tool (GUI)

1. **Download** [Flash Download Tools](https://www.espressif.com/en/support/download/other-tools)
2. **Run** the tool and select "ESP32-P4"
3. **Configure:**
   - Load `.bin` file at address: `0x0`
   - SPI Speed: 40MHz
   - SPI Mode: DIO
   - Flash Size: 32MB
4. **Select COM Port** from dropdown
5. **Click START** and wait for "FINISH" message

### Step 3: First Boot

After flashing successfully:
1. **Disconnect** USB cable
2. **Reconnect** USB cable (or power via external supply)
3. **Display shows:** QR code and WiFi network name

Proceed to [First Boot & WiFi Setup](#first-boot--wifi-setup) section.

---

## Compiling From Source

**Recommended for:** 4.0" display users, developers, customization, latest features

### Prerequisites

- **Arduino IDE** 1.8.19+ or **PlatformIO**
- **ESP32 Arduino Core** 3.3.4+
- **USB cable** for programming

### Method 1: PowerShell Script (Windows - Easiest)

The `compile-and-upload.ps1` script automatically handles library patching, compilation, and upload.

**If device is connected via USB:**
```powershell
.\compile-and-upload.ps1 -ComPort COM3
```

**If device is NOT connected (compile only, then upload via OTA later):**
```powershell
.\compile-and-upload.ps1 -SkipUpload -OutputPath "C:\Users\YourName\Desktop\firmware"
```

**What the script does:**
- Automatically patches Arduino_GFX library for ESP32-P4 compatibility
- Injects git commit hash into `build_info.h`
- Detects Arduino CLI installation
- Compiles firmware with correct board settings
- Reports memory usage
- Uploads to device (if `-SkipUpload` not specified)

### Method 2: Arduino IDE (Manual Setup)

#### 1. Install ESP32 Board Support

**Add Board Manager URL:**
1. File → Preferences → Additional Board Manager URLs
2. Add: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Click OK

**Install ESP32 Platform:**
1. Tools → Board → Boards Manager
2. Search: "esp32"
3. Install: "esp32 by Espressif Systems" **version 3.3.4 or higher**

#### 2. Install Required Libraries

Open **Sketch → Include Library → Manage Libraries**, then search and install:

| Library | Author | Version |
|---------|--------|---------|
| GFX Library for Arduino | moononournation | **1.6.0** |
| JPEGDEC | bitbank2 | **1.8.2** |
| PubSubClient | Nick O'Leary | **2.8** |
| WebSockets | Markus Sattler | **2.7.1** |
| ElegantOTA | Ayush Sharma | **3.1.7** |
| ArduinoJson | Benoit Blanchon | **7.2.1** |

#### 3. ⚠️ Apply Critical GFX Library Patch

The GFX Library for Arduino requires a one-line patch to work with ESP32-P4. **Without this patch, compilation will fail.**

**Background:** ESP-IDF 5.5+ changed the clock source enum type for ESP32-P4, causing a type mismatch with `MIPI_DSI_PHY_CLK_SRC_DEFAULT`.

**Manual Patch Steps:**

1. **Locate library folder:**
   - **Windows (Arduino IDE):** `%USERPROFILE%\Documents\Arduino\libraries\GFX_Library_for_Arduino\`
   - **Windows (Arduino15):** `%LOCALAPPDATA%\Arduino15\libraries\GFX_Library_for_Arduino\`
   - **macOS/Linux:** `~/Arduino/libraries/GFX_Library_for_Arduino/`

2. **Open file:** `src/databus/Arduino_ESP32DSIPanel.cpp`

3. **Find line** (around line 70-80):
   ```cpp
   .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
   ```

4. **Replace with:**
   ```cpp
   .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, //MIPI_DSI_PHY_CLK_SRC_DEFAULT,
   ```

5. **Save** file and restart Arduino IDE

**Automated Patch (PowerShell - Windows):**
```powershell
(Get-Content "$env:USERPROFILE\Documents\Arduino\libraries\GFX_Library_for_Arduino\src\databus\Arduino_ESP32DSIPanel.cpp") -replace '.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,', '.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, //MIPI_DSI_PHY_CLK_SRC_DEFAULT,' | Set-Content "$env:USERPROFILE\Documents\Arduino\libraries\GFX_Library_for_Arduino\src\databus\Arduino_ESP32DSIPanel.cpp"
```

**Automated Patch (Bash - macOS/Linux):**
```bash
sed -i.bak 's/.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,/.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, \/\/MIPI_DSI_PHY_CLK_SRC_DEFAULT,/' ~/Arduino/libraries/GFX_Library_for_Arduino/src/databus/Arduino_ESP32DSIPanel.cpp
```

**Technical Details:**
- Root cause: Type mismatch in `esp32p4/include/soc/clk_tree_defs.h` (ESP32 Arduino Core 3.3.4+ with IDF 5.5.1+)
- Original issue: [LovyanGFX Issue #788](https://github.com/lovyan03/LovyanGFX/issues/788)
- Same issue affects Arduino_GFX library

#### 4. Configure Board Settings

![Arduino IDE Configuration](../images/ArduinoIDE.jpg)

**Tools Menu Settings:**
- **Board:** ESP32-P4-Function-EV-Board
- **Flash Size:** 32MB
- **Partition Scheme:** 13MB app / 7MB data (32MB) or select "Custom" and use included `partitions.csv`
- **PSRAM:** **Enabled** (required - firmware will not run without PSRAM)
- **Upload Speed:** 921600

#### 5. Select Display Size (Optional)

**For 4.0" Display Users:**

Edit `displays_config.h`:
```cpp
#define SCREEN_3INCH_4_DSI 1
#define SCREEN_4INCH_DSI 2

#ifndef CURRENT_SCREEN
#define CURRENT_SCREEN SCREEN_4INCH_DSI  // <-- Change from SCREEN_3INCH_4_DSI
#endif
```

**After changing:**
1. Delete `build/` folder (if exists)
2. Recompile entire project

#### 6. Compile & Upload

**Upload to Device:**
1. Connect ESP32-P4 via USB-C
2. Select correct COM port: Tools → Port → COM# (ESP32-P4)
3. Click Upload button (→) or press **Ctrl+U**
4. Wait for "Hard resetting via RTS pin..." message

**Export Binary Only (for OTA upload later):**
1. Sketch → Export Compiled Binary (**Ctrl+Alt+S**)
2. Look for: `ESP32-P4-Allsky-Display.ino.esp32p4.bin` in sketch folder
3. Use for OTA updates via web interface

### Method 3: PlatformIO

Create `platformio.ini` in project root:

```ini
[env:esp32-p4-function-ev-board]
platform = espressif32 @ ^6.5.0
board = esp32-p4-function-ev-board
framework = arduino

; Board configuration
board_build.partitions = partitions.csv
board_build.flash_size = 32MB
board_build.psram_type = opi
board_build.memory_type = opi_opi

; Build flags
build_flags = 
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

; Upload settings
upload_speed = 921600
monitor_speed = 9600

; Dependencies
lib_deps = 
    moononournation/GFX Library for Arduino @ 1.6.0
    bitbank2/JPEGDEC @ 1.8.2
    knolleary/PubSubClient @ 2.8
    links2004/WebSockets @ 2.7.1
    ayushsharma82/ElegantOTA @ 3.1.7
    bblanchon/ArduinoJson @ 7.2.1

; Extra scripts for library patching
extra_scripts = pre:scripts/patch_arduino_gfx.py
```

**Build and upload:**
```bash
pio run --target upload
```

---

## First Boot & WiFi Setup

After flashing firmware, the device creates an open WiFi network for easy configuration.

### Step 1: Connect to WiFi Access Point

**Network Name:** `AllSky-Display-Setup`  
**Password:** None (open network)

![WiFi Setup QR Code](../images/config-qr-ap-setup.jpg)

1. **Connect** your phone/computer to `AllSky-Display-Setup` WiFi network
2. **Scan QR Code** shown on display, OR
3. **Open browser** to `http://192.168.4.1`

### Step 2: Configure WiFi Credentials

You'll see a captive portal page with:
- List of detected WiFi networks
- Signal strength indicators
- Password input field

**Configure:**
1. **Select your WiFi network** from the list
2. **Enter password**
3. **Click "Connect"** button

### Step 3: Device Connects & Reboots

After submitting credentials:
1. **Device validates** connection (may take 10-20 seconds)
2. **Display shows** connection progress
3. **Device reboots** automatically
4. **Display shows** device IP address

### Step 4: Access Web Interface

Open browser to: `http://[device-ip]:8080/`

**Alternative (mDNS):** `http://allskyesp32.lan:8080/`

**If you forget the IP:**
- Check display (shows IP in corner or status screen)
- Check router's DHCP client list
- Use mDNS hostname: `allskyesp32.lan`
- Connect via USB and check serial monitor (9600 baud)

---

## Hardcoded WiFi Configuration (Advanced)

**When to use:**
- Factory provisioning of multiple devices
- Deploying to closed/private networks
- Automated builds for specific deployment

**Not recommended for:** Regular users (use captive portal instead)

### Edit Default Configuration

**File:** `config_storage.cpp` → `setDefaults()` function

```cpp
void ConfigStorage::setDefaults() {
    // ... existing code ...
    
    // Hardcode WiFi credentials
    config.wifiProvisioned = true;                    // Skip captive portal
    strncpy(config.wifiSSID, "YOUR_WIFI_SSID", 32);  // Your network name
    strncpy(config.wifiPassword, "YOUR_PASSWORD", 64); // Your network password
    
    // Optional: Pre-configure MQTT
    strncpy(config.mqttServer, "192.168.1.250", 64);
    config.mqttPort = 1883;
    strncpy(config.mqttUser, "", 32);
    strncpy(config.mqttPassword, "", 64);
    
    // ... rest of defaults ...
}
```

### Optional: Pre-configure Image Sources

**File:** `config.cpp`

```cpp
const char* DEFAULT_IMAGE_SOURCES[] = {
    "http://your-allsky-server.com/resized/image.jpg",
    "http://camera2.example.com/image.jpg",
    // Add up to 10 sources
};
```

### Compile & Upload

After editing configuration:
1. Follow [Compiling From Source](#compiling-from-source) steps
2. Upload firmware to device
3. Device boots directly to configured WiFi (no captive portal)

### Verify Connection

**Serial Monitor (9600 baud) shows:**
```
[WiFi] Connecting to YOUR_WIFI_SSID...
[WiFi] Connected! IP: 192.168.1.123
[mDNS] Hostname: allskyesp32.lan
```

Access web UI at displayed IP address.

### Security Warning

**Do NOT commit credentials to version control!**

Add to `.gitignore`:
```
config_storage.cpp
config.cpp
```

Or use environment variables/build-time injection for CI/CD pipelines.

### Reset to Captive Portal Mode

To return to captive portal setup:
1. **Web interface:** Factory Reset button, OR
2. **Serial command:** Press `F` in serial monitor (9600 baud), OR
3. **Erase flash:** `esptool.py erase_flash` (requires USB connection)

---

## Troubleshooting Installation

### Compilation Issues

| Issue | Solution |
|-------|----------|
| **`MIPI_DSI_PHY_CLK_SRC_DEFAULT` error** | Apply GFX library patch (see step 3 above) |
| **"PSRAM not found" error** | Enable PSRAM in Tools → PSRAM → Enabled |
| **Library not found** | Verify all 6 libraries installed with exact versions |
| **Partition table error** | Ensure `partitions.csv` is in sketch folder |
| **Out of memory during compile** | Close other applications, increase Arduino IDE memory in preferences |

### Upload/Flash Issues

| Issue | Solution |
|-------|----------|
| **Port not found** | Install CH340 USB driver, check Device Manager |
| **Upload fails midway** | Lower upload speed to 460800 or 115200 |
| **Device not entering bootloader** | Hold BOOT button while connecting USB |
| **esptool can't connect** | Try: `--before default_reset --after hard_reset` flags |
| **Wrong chip detected** | Explicitly specify: `--chip esp32p4` |

### WiFi Connection Issues

| Issue | Solution |
|-------|----------|
| **Captive portal doesn't appear** | Manually browse to `http://192.168.4.1` |
| **Can't find AllSky-Display-Setup network** | Wait 60s after boot, device may still be initializing |
| **Connection fails after entering password** | Verify password case-sensitive, ensure 2.4GHz network (5GHz not supported) |
| **Device shows IP but can't access web** | Check firewall, verify same subnet, try mDNS: `allskyesp32.lan` |
| **Hardcoded WiFi not connecting** | Check serial monitor (9600 baud) for error messages, verify SSID/password exact match |

### Display Issues

| Issue | Solution |
|-------|----------|
| **Display stays blank** | Verify GFX library patch applied, check power supply (need 2A), inspect DSI cable connection |
| **Wrong colors/corrupted image** | Wrong display selected in `displays_config.h`, recompile for correct display |
| **Backlight not working** | Check GPIO26 connection, verify PWM settings in `config.h` |
| **Touch not responsive** | Verify I2C connections (SDA/SCL), check touch controller address in serial logs |

### Memory Issues

| Issue | Solution |
|-------|----------|
| **Boot loop / constant resets** | PSRAM not enabled or defective, verify board settings |
| **"Out of memory" in serial logs** | Check buffer sizes in `config.h`, verify images under 4MB |
| **Watchdog timeout on startup** | Normal during first image download (can take 30s), wait longer |

### Getting Help

If issues persist:
1. **Check serial monitor** at 9600 baud for detailed error messages
2. **Enable debug logging** in `logging.h` (set `LOG_LEVEL_DEBUG`)
3. **Collect logs** and create [GitHub Issue](https://github.com/chvvkumar/ESP32-P4-Allsky-Display/issues)
4. **Include:**
   - Exact error message
   - Board settings from Tools menu
   - Library versions
   - Display model (3.4" or 4.0")
   - Serial monitor output

---

## Next Steps

- **[Configuration Guide](03_configuration.md)** - Set up images, MQTT, and display settings
- **[Features & Usage](04_features.md)** - Learn about all features and controls
- **[OTA Updates](05_ota_updates.md)** - Wireless firmware updates
