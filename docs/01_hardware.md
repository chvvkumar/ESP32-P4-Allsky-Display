# Hardware Requirements & Setup

## Table of Contents
- [Supported Displays](#supported-displays)
- [Required Hardware](#required-hardware)
- [Pin Assignments](#pin-assignments)
- [Display Configuration](#display-configuration)
- [Required Libraries](#required-libraries)
- [Library Installation](#library-installation)
- [Hardware Wiring](#hardware-wiring)
- [3D Printed Case](#3d-printed-case)

---

## Supported Displays

Two MIPI DSI displays are supported (compile-time selection via `CURRENT_SCREEN` define):

### Option 1: Waveshare 3.4" ESP32-P4 Touch LCD (800×800)
✅ **Tested & Confirmed Working**

- **Purchase:** [Waveshare Website](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm)
- **Panel:** MIPI DSI 800×800 RGB565
- **Driver IC:** Vendor-specific (init sequences in `displays_config.h`)
- **Touch:** GT911 capacitive touch controller (optional)
- **Backlight:** PWM-controlled via GPIO26
- **Connector:** FPC 40-pin (display) + 6-pin (touch I2C)

### Option 2: Waveshare 4.0" ESP32-P4 Touch LCD (720×720)
⚠️ **Untested - Configuration Available**

- **Purchase:** [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4C)
- **Panel:** MIPI DSI 720×720 RGB565
- **Driver IC:** Different init sequence from 3.4"
- **Touch:** GT911 capacitive touch controller (optional)
- **Backlight:** PWM-controlled via GPIO26
- **Connector:** Same as 3.4" option

**Important:** This project has been developed and tested exclusively on the 3.4" display. The 4.0" display configuration is available in the code but has not been verified on actual hardware.

**Display Selection:**
```cpp
// In displays_config.h
#define SCREEN_3INCH_4_DSI 1
#define SCREEN_4INCH_DSI 2

#ifndef CURRENT_SCREEN
#define CURRENT_SCREEN SCREEN_3INCH_4_DSI  // <-- Change this
#endif
```

---

## Required Hardware

### Microcontroller: ESP32-P4-Function-EV-Board

**Specifications:**
- **MCU:** ESP32-P4 (RISC-V dual-core, 360MHz)
- **RAM:** 512KB SRAM + 32MB PSRAM (external)
- **Flash:** 32MB (supports OTA with A/B partitions)
- **Display Interface:** MIPI DSI (2-lane, up to 1GHz)
- **Connectivity:** WiFi 6
- **Touch Interface:** GT911 via I2C
- **USB:** USB-OTG for programming and debugging

**Why ESP32-P4?**
- **PPA Acceleration:** 10-50× faster image scaling/rotation vs. software (385-507ms render time)
- **Large PSRAM:** Required for 4MB+ image buffers and display framebuffer
- **DSI Interface:** Direct high-speed connection to display (no SPI bottleneck)
- **Dual-core:** Allows concurrent operations (network, display, processing)

**Requirements:** 16MB flash minimum, **PSRAM is mandatory** (firmware cannot run without it)

### Touch Controller (Optional)

**Goodix GT911** - 5-point capacitive touch
- **Interface:** I2C (address 0x5D or 0x14, auto-detected)
- **Pins:** SDA, SCL, INT (interrupt), RST (reset)
- **Features:**
  - Multi-touch support (firmware uses single-touch only)
  - Hardware gesture detection (not used)
  - Configurable resolution mapping

**Gesture Mapping:**
- **Single Tap:** Advance to next image (in cycling mode)
- **Double Tap (400ms window):** Toggle cycling on/off

---

## Pin Assignments

### Display Interface (MIPI DSI)

| Signal | ESP32-P4 GPIO | Description |
|--------|---------------|-------------|
| **DSI_D0_P** | GPIO0 | Data lane 0 positive |
| **DSI_D0_N** | GPIO1 | Data lane 0 negative |
| **DSI_D1_P** | GPIO2 | Data lane 1 positive |
| **DSI_D1_N** | GPIO3 | Data lane 1 negative |
| **DSI_CLK_P** | GPIO4 | Clock lane positive |
| **DSI_CLK_N** | GPIO5 | Clock lane negative |

**Note:** MIPI DSI uses differential signaling - do NOT connect these pins to anything else.

### Backlight Control

| Signal | ESP32-P4 GPIO | Description | Config |
|--------|---------------|-------------|--------|
| **LCD_BL_PWM** | GPIO26 | Backlight PWM | 5kHz, 10-bit, Channel 0 |

```cpp
// Brightness control configuration (config.h)
#define BACKLIGHT_PIN 26
#define BACKLIGHT_CHANNEL 0
#define BACKLIGHT_FREQ 5000        // 5kHz PWM
#define BACKLIGHT_RESOLUTION 10    // 10-bit (0-1023)
#define DEFAULT_BRIGHTNESS 50      // 50% default
```

### Touch Controller (I2C)

**3.4" Display Configuration:**
| Signal | ESP32-P4 GPIO | Description |
|--------|---------------|-------------|
| **SDA** | GPIO8 | I2C data line |
| **SCL** | GPIO18 | I2C clock line |
| **GT_INT** | GPIO9 (optional) | Touch interrupt |
| **GT_RST** | GPIO7 (optional) | Touch reset |

**4.0" Display Configuration:**
| Signal | ESP32-P4 GPIO | Description |
|--------|---------------|-------------|
| **SDA** | GPIO8 | I2C data line |
| **SCL** | GPIO9 | I2C clock line |
| **GT_INT** | GPIO7 (optional) | Touch interrupt |
| **GT_RST** | GPIO18 (optional) | Touch reset |

**I2C Configuration:**
```cpp
// Configured in displays_config.h per display
i2c_sda_pin: 8       // Data line
i2c_scl_pin: 18      // Clock line (3.4" display)
             9       // Clock line (4.0" display)
i2c_clock_speed: 100000  // 100kHz (standard mode)
```

### Power Supply

| Rail | Voltage | Current | Notes |
|------|---------|---------|-------|
| **VDD** | 3.3V | ~500mA | ESP32-P4 core power |
| **VDDIO** | 3.3V | ~200mA | I/O voltage |
| **Display** | 3.3V | ~300mA | Panel and backlight (varies with brightness) |
| **Touch** | 3.3V | ~10mA | GT911 power |

**Total:** ~1A @ 3.3V (recommend 2A supply for margin)

---

## Display Configuration

### Timing Parameters

Display timing is configured in `displays_config.h` using vendor-specific parameters:

```cpp
struct DisplayConfig {
    const char* name;
    
    // Horizontal timing
    uint32_t hsync_pulse_width;   // Horizontal sync pulse width
    uint32_t hsync_back_porch;    // Horizontal back porch
    uint32_t hsync_front_porch;   // Horizontal front porch
    
    // Vertical timing
    uint32_t vsync_pulse_width;   // Vertical sync pulse width
    uint32_t vsync_back_porch;    // Vertical back porch
    uint32_t vsync_front_porch;   // Vertical front porch
    
    // DSI parameters
    uint32_t prefer_speed;        // Preferred pixel clock speed
    uint32_t lane_bit_rate;       // Lane bit rate (bps)
    
    // Display geometry
    uint16_t width;               // Display width (800 or 720)
    uint16_t height;              // Display height (800 or 720)
    int8_t rotation;              // Rotation (0, 1, 2, 3 for 0°, 90°, 180°, 270°)
    bool auto_flush;              // Auto flush framebuffer
    
    // Panel control pins
    int8_t rst_pin;               // Reset pin (typically -1, no reset)
    
    // I2C configuration for touch
    int8_t i2c_sda_pin;
    int8_t i2c_scl_pin;
    uint32_t i2c_clock_speed;
    int8_t lcd_rst;               // LCD reset (separate from controller reset)
};
```

**3.4" Display Example:**
```cpp
{
    .name = "3.4inch Round Display",
    .hsync_pulse_width = 10,
    .hsync_back_porch = 10,
    .hsync_front_porch = 120,
    .vsync_pulse_width = 10,
    .vsync_back_porch = 10,
    .vsync_front_porch = 20,
    .prefer_speed = 80000000,  // 80MHz pixel clock
    .lane_bit_rate = 1000000000,  // 1Gbps per lane
    .width = 800,
    .height = 800,
    .rotation = 0,
    .auto_flush = false,
    .rst_pin = -1,
    .i2c_sda_pin = 8,
    .i2c_scl_pin = 18,
    .i2c_clock_speed = 100000,
    .lcd_rst = -1
}
```

### Vendor Init Sequences

Each display requires a vendor-specific initialization sequence sent via MIPI DCS (Display Command Set):

```cpp
static const lcd_init_cmd_t vendor_specific_init_default[] = {
    {0xE0, (uint8_t[]){0x00}, 1, 0},  // Command 0xE0, data 0x00, 1 byte, no delay
    {0xE1, (uint8_t[]){0x93}, 1, 0},  // Enable extended commands
    // ... hundreds of vendor-specific register writes ...
    {0x11, NULL, 0, 120},             // Sleep out, 120ms delay
    {0x29, NULL, 0, 20},              // Display on, 20ms delay
};
```

**Critical:** These sequences are panel-specific and must match your hardware. Incorrect sequences can cause:
- Blank screen (display doesn't turn on)
- Corrupted colors (wrong color mode)
- Timing issues (screen flicker or tearing)

---

## Required Libraries

### Arduino_GFX (Display)

**Repository:** https://github.com/moononournation/Arduino_GFX  
**Version:** 1.6.0 (required)

**Purpose:** Hardware-accelerated graphics library with MIPI DSI support for ESP32-P4.

**Key Features:**
- Direct DSI panel control (no framebuffer copy overhead)
- RGB565 color format (native to display)
- DMA-based pixel transfers
- Multiple panel driver support

**Installation:**
```
Arduino IDE: Library Manager → Search "GFX Library for Arduino" by moononournation → Install 1.6.0
Arduino CLI: arduino-cli lib install "GFX Library for Arduino@1.6.0"
PlatformIO: lib_deps = moononournation/GFX Library for Arduino @ 1.6.0
```

**⚠️ Critical Patch Required:**

The library has a type mismatch with ESP-IDF 5.5+. You must apply this patch to `Arduino_ESP32DSIPanel.cpp`:

**File:** `Arduino15/libraries/GFX_Library_for_Arduino/src/display/Arduino_ESP32DSIPanel.cpp`

**Change (around line 150):**
```cpp
// BEFORE (causes compilation error)
.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,

// AFTER (corrected)
.phy_clk_src = MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M,
```

**Reason:** ESP-IDF 5.5 changed the enum type for `phy_clk_src`. The old constant name is no longer compatible with ESP32-P4.

**Background:** ESP-IDF 5.5+ changed the clock source enum type for ESP32-P4, causing `MIPI_DSI_PHY_CLK_SRC_DEFAULT` (which resolves to `SOC_MOD_CLK_PLL_F20M` of type `soc_module_clk_t`) to be incompatible with the expected `mipi_dsi_phy_pllref_clock_source_t` type. The workaround is to use the correctly-typed constant `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M` directly.

**Technical Details:** 
- Root cause: Type mismatch in `esp32p4/include/soc/clk_tree_defs.h` (ESP32 Arduino Core 3.3.4+ with IDF 5.5.1+)
- Original issue: [LovyanGFX Issue #788](https://github.com/lovyan03/LovyanGFX/issues/788)
- Same issue affects Arduino_GFX library

**Auto-Patch Script:** The `compile-and-upload.ps1` script automatically applies this patch before compilation.

### JPEGDEC (Image Decoding)

**Repository:** https://github.com/bitbank2/JPEGDEC  
**Version:** 1.8.2 (required)

**Purpose:** Fast JPEG decoder optimized for embedded systems.

**Key Features:**
- MCU-optimized (no floating point)
- Supports progressive JPEGs (partially)
- Direct decode to RGB565
- Callback-based pixel delivery (no intermediate buffer)

**Limitations:**
- **PNG not supported** - Will fail with "Invalid JPEG header" error
- **Large images** - Max 1448×1448 (limited by `FULL_IMAGE_BUFFER_SIZE`)
- **Some JPEG formats** may fail (CMYK, unusual subsampling)

**Installation:**
```
Arduino IDE: Library Manager → Search "JPEGDEC" by bitbank2 → Install 1.8.2
Arduino CLI: arduino-cli lib install "JPEGDEC@1.8.2"
PlatformIO: lib_deps = bitbank2/JPEGDEC @ 1.8.2
```

### PubSubClient (MQTT)

**Repository:** https://github.com/knolleary/pubsubclient  
**Version:** 2.8 (required)

**Purpose:** MQTT client for Home Assistant integration.

**Configuration:**
```cpp
// Increase buffer sizes for HA discovery messages (default 128 is too small)
#define MQTT_MAX_PACKET_SIZE 2048
#define MQTT_KEEPALIVE 60
```

**Installation:**
```
Arduino IDE: Library Manager → Search "PubSubClient" by Nick O'Leary → Install 2.8
Arduino CLI: arduino-cli lib install "PubSubClient@2.8"
PlatformIO: lib_deps = knolleary/PubSubClient @ 2.8
```

### WebSocketsServer

**Repository:** https://github.com/Links2004/arduinoWebSockets  
**Version:** 2.7.1 (required)

**Purpose:** WebSocket support for real-time console.

**Installation:**
```
Arduino IDE: Library Manager → Search "WebSockets" by Markus Sattler → Install 2.7.1
Arduino CLI: arduino-cli lib install "WebSockets@2.7.1"
PlatformIO: lib_deps = links2004/WebSockets @ 2.7.1
```

### ElegantOTA

**Repository:** https://github.com/ayushsharma82/ElegantOTA  
**Version:** 3.1.7 (required)

**Purpose:** Web-based OTA firmware upload UI.

**Features:**
- Drag-and-drop binary upload
- Progress bar with percentage
- Auto-reboot after successful upload
- Partition validation

**Installation:**
```
Arduino IDE: Library Manager → Search "ElegantOTA" by Ayush Sharma → Install 3.1.7
Arduino CLI: arduino-cli lib install "ElegantOTA@3.1.7"
PlatformIO: lib_deps = ayushsharma82/ElegantOTA @ 3.1.7
```

### ArduinoJson

**Repository:** https://github.com/bblanchon/ArduinoJson  
**Version:** 7.2.1 (required)

**Purpose:** JSON parsing and serialization for API responses and configuration.

**Features:**
- Efficient memory management
- Support for nested objects and arrays
- Zero-copy string handling
- Type-safe API

**Installation:**
```
Arduino IDE: Library Manager → Search "ArduinoJson" by Benoit Blanchon → Install 7.2.1
Arduino CLI: arduino-cli lib install "ArduinoJson@7.2.1"
PlatformIO: lib_deps = bblanchon/ArduinoJson @ 7.2.1
```

### Built-in Libraries (ESP32 Core)

The following libraries are built into the ESP32 Arduino Core and require no installation:

- **WebServer** - HTTP server for configuration UI (port 8080)
- **HTTPClient** - HTTP client for image downloads
- **Preferences** - NVS (Non-Volatile Storage) wrapper for configuration persistence

---

## Library Installation

### Method 1: Arduino IDE (Recommended for Beginners)

1. **Install ESP32 Board Support:**
   - File → Preferences → Additional Board Manager URLs:
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Tools → Board → Boards Manager → Search "esp32" → Install "esp32 by Espressif Systems" **version 3.3.4**

2. **Select Board:**
   - Tools → Board → ESP32 Arduino → ESP32-P4-Function-EV-Board

3. **Install Libraries:**
   - Sketch → Include Library → Manage Libraries
   - Search and install the following libraries with exact versions:
     - **GFX Library for Arduino** by moononournation → **1.6.0**
     - **JPEGDEC** by bitbank2 → **1.8.2**
     - **PubSubClient** by Nick O'Leary → **2.8**
     - **WebSockets** by Markus Sattler → **2.7.1**
     - **ElegantOTA** by Ayush Sharma → **3.1.7**
     - **ArduinoJson** by Benoit Blanchon → **7.2.1**

4. **Apply GFX Library Patch:**
   - Manually edit `Arduino_ESP32DSIPanel.cpp` as described above
   - **OR** use the provided `compile-and-upload.ps1` script (auto-patches)

### Method 2: PlatformIO

Create `platformio.ini`:
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

### Method 3: Manual Installation

1. Download libraries as ZIP from GitHub
2. Sketch → Include Library → Add .ZIP Library
3. Repeat for each library
4. Apply GFX patch manually

---

## Hardware Wiring

### Minimal Setup (Display Only)

```
ESP32-P4 Function EV Board
│
├── MIPI DSI Connector (40-pin FPC)
│   ├── D0+/D0-  →  Display Data Lane 0
│   ├── D1+/D1-  →  Display Data Lane 1
│   ├── CLK+/CLK- →  Display Clock Lane
│   ├── VDD 3.3V →  Display Power
│   └── GND      →  Ground
│
└── GPIO26 (LCD_BL_PWM)  →  Backlight Control (via transistor/FET)
```

**Backlight Circuit:**
```
GPIO26 ──────┬──────[1kΩ]──────┐
             │                 │
             │            [FET Gate]
             │                 │
             └─────[10kΩ]──── GND
                               │
3.3V ────────[LED+ Display]────┤
                               │
                          [FET Drain]
                               │
                              GND
```

### Full Setup (Display + Touch)

```
ESP32-P4 Function EV Board
│
├── MIPI DSI Connector (Display)
│   └── [Same as minimal setup]
│
├── I2C Touch Connector (6-pin)
│   ├── GPIO8  (SDA)  →  GT911 SDA
│   ├── GPIO18 (SCL)  →  GT911 SCL (3.4" display)
│   │   GPIO9  (SCL)  →  GT911 SCL (4.0" display)
│   ├── GPIO9  (INT)  →  GT911 Interrupt (optional)
│   ├── GPIO7  (RST)  →  GT911 Reset (optional)
│   ├── 3.3V          →  GT911 Power
│   └── GND           →  Ground
│
└── USB-C (Programming/Power)
    └── Connected to PC for programming
```

**I2C Pull-ups:**
- Most displays have built-in 4.7kΩ pull-ups on SDA/SCL
- If not present, add external 4.7kΩ resistors to 3.3V

---

## 3D Printed Case

**Optional Desk Stand:**

![3D Printed Case](../images/Case.jpg)

**Download:** [Printables.com - Desk Stand for ESP32-P4 Display](https://www.printables.com/model/1352883-desk-stand-for-waveshare-esp32-p4-wifi6-touch-lcd)

**Features:**
- Vertical or angled display positioning
- Cable management cutouts
- Access to USB-C port for power/programming
- Secure mounting for Waveshare 3.4" display

---

## Memory Configuration & Buffer Sizes

### Partition Scheme

The firmware uses a custom partition layout for OTA support:

**File:** `partitions.csv` (in sketch folder)
```csv
# Name,   Type, SubType, Offset,  Size,     Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0xA00000,  # 10MB
app1,     app,  ota_1,   0xA10000,0xA00000,  # 10MB
spiffs,   data, spiffs,  0x1410000,0x6F0000, # 7MB
coredump, data, coredump,0x1B00000,0x64000,
```

**Layout:**
- **NVS (20KB):** Configuration storage
- **OTA Data (8KB):** Tracks active partition
- **App0 (10MB):** Primary firmware slot
- **App1 (10MB):** Secondary firmware slot (OTA target)
- **SPIFFS (7MB):** File system (unused currently)
- **Core Dump (400KB):** Crash dumps

**OTA Process:**
1. Upload new firmware to inactive partition (app0 or app1)
2. Bootloader validates firmware
3. On successful validation, marks new partition as active
4. Reboots to new firmware
5. If new firmware fails to boot, bootloader auto-reverts to previous partition

### Buffer Sizes (config.h)

```cpp
// Controls ALL memory allocations - adjust carefully
#define IMAGE_BUFFER_MULTIPLIER 1       // 1× display size for downloads
#define FULL_IMAGE_BUFFER_SIZE (4 * 1024 * 1024)  // 4MB for large images
#define SCALED_BUFFER_MULTIPLIER 4      // 4× display for PPA (max 2.0× scale)
```

**Buffer Calculations:**
- **Image Buffer:** 800 × 800 × 2 × `IMAGE_BUFFER_MULTIPLIER` = 1.28 MB
- **Full Image Buffer:** Fixed 4MB (supports up to 1448×1448 images)
- **Pending Buffer:** Same as Full (4MB, for flicker-free double buffering)
- **Scaled Buffer:** 800 × 800 × 2 × `SCALED_BUFFER_MULTIPLIER` = 5.12 MB

**Total:** ~15.68 MB allocated from 32MB PSRAM

**Increasing Max Scale:**
To support 3.0× scale instead of 2.0×:
```cpp
#define SCALED_BUFFER_MULTIPLIER 9  // sqrt(9) = 3.0×
```
This increases scaled buffer to 11.52 MB (total ~22 MB used).

---

## Summary

The ESP32-P4 AllSky Display requires:
- **Hardware:** ESP32-P4 board, MIPI DSI display (3.4" or 4.0"), optional GT911 touch
- **Wiring:** DSI differential pairs, I2C for touch, PWM backlight control
- **Libraries:** Arduino_GFX (patched), JPEGDEC, PubSubClient, WebSockets, ElegantOTA, ArduinoJson
- **Configuration:** Display selection at compile-time, buffer sizes tuned for performance

**Critical Points:**
- **Arduino_GFX patch is mandatory** - code won't compile without it
- **Buffer allocation order matters** - allocate before display init
- **PSRAM is essential** - firmware cannot run without 32MB PSRAM
- **Display selection requires recompile** - no runtime switching

All libraries are open-source and actively maintained. The hardware is readily available from ESP32 suppliers.

---

## Next Steps

- **[Installation Guide](02_installation.md)** - Flash firmware and configure WiFi
- **[Configuration Guide](03_configuration.md)** - Set up images, MQTT, and display settings
- **[Developer Documentation](developer/architecture.md)** - Deep dive into system architecture
