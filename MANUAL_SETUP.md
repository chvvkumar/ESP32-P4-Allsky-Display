# Manual Setup Guide

**When to use this:** Compiling from source, hardcoded WiFi credentials, advanced deployments

**Recommended for most users:** Flash pre-compiled binary and use captive portal (see [README](README.md))

---

## Compiling From Source

### Prerequisites

- **Arduino IDE** 1.8.19+ or **PlatformIO**
- **ESP32 Arduino Core** 3.3.4+
- **Required Libraries:** GFX Library for Arduino (1.6.3+), JPEGDEC (1.8.4+), PubSubClient (2.8.0+), ElegantOTA

### Library Installation & Patching

**Install Required Libraries:**

Via Arduino IDE Library Manager:
1. Sketch → Include Library → Manage Libraries
2. Search and install:
   - GFX Library for Arduino (1.6.0+)
   - JPEGDEC (1.8.2+)
   - PubSubClient (2.8.0+)
   - ElegantOTA (3.1.7+)

**⚠️ Critical: Patch GFX Library for ESP32-P4 Compatibility**

The GFX Library for Arduino (Arduino_GFX by moononournation) requires a one-line patch to work with ESP32-P4. Without this patch, compilation will fail with `MIPI_DSI_PHY_CLK_SRC_DEFAULT` type conversion errors.

**Background:** ESP-IDF 5.5+ changed the clock source enum type for ESP32-P4, causing `MIPI_DSI_PHY_CLK_SRC_DEFAULT` (which resolves to `SOC_MOD_CLK_PLL_F20M` of type `soc_module_clk_t`) to be incompatible with the expected `mipi_dsi_phy_pllref_clock_source_t` type. The workaround is to use the correctly-typed constant `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M` directly.

**Technical Details:** 
- Root cause: Type mismatch in `esp32p4/include/soc/clk_tree_defs.h` (ESP32 Arduino Core 3.3.4+ with IDF 5.5.1+)
- Original issue: [LovyanGFX Issue #788](https://github.com/lovyan03/LovyanGFX/issues/788)
- Same issue affects Arduino_GFX library

**Manual Patch Steps:**

1. Locate the GFX Library installation folder:
   - **Windows (Arduino IDE):** `%USERPROFILE%\Documents\Arduino\libraries\GFX_Library_for_Arduino\`
   - **Windows (Arduino15):** `%LOCALAPPDATA%\Arduino15\libraries\GFX_Library_for_Arduino\`
   - **macOS/Linux:** `~/Arduino/libraries/GFX_Library_for_Arduino/`

2. Open file: `src/databus/Arduino_ESP32DSIPanel.cpp`

3. Find line (around line 70-80):
   ```cpp
   .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
   ```

4. Replace with:
   ```cpp
   .phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, //MIPI_DSI_PHY_CLK_SRC_DEFAULT,
   ```

5. Save file and restart Arduino IDE

**What this does:** Uses the correct enum type (`MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M`) which is functionally equivalent to the macro value but with the proper type expected by the ESP32-P4 DSI driver structure.

**Automated Patch (PowerShell):**

Windows users can use this one-liner in PowerShell:
```powershell
(Get-Content "$env:USERPROFILE\Documents\Arduino\libraries\GFX_Library_for_Arduino\src\databus\Arduino_ESP32DSIPanel.cpp") -replace '.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,', '.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, //MIPI_DSI_PHY_CLK_SRC_DEFAULT,' | Set-Content "$env:USERPROFILE\Documents\Arduino\libraries\GFX_Library_for_Arduino\src\databus\Arduino_ESP32DSIPanel.cpp"
```

**Automated Patch (Bash):**

macOS/Linux users can use this one-liner:
```bash
sed -i.bak 's/.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,/.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M, \/\/MIPI_DSI_PHY_CLK_SRC_DEFAULT,/' ~/Arduino/libraries/GFX_Library_for_Arduino/src/databus/Arduino_ESP32DSIPanel.cpp
```

**Note:** The PowerShell compile script (`compile-and-upload.ps1`) automatically applies this patch, so if using the script, manual patching is not needed.

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
