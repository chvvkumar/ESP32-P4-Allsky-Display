# ESP32-P4 AllSky Display - AI Agent Instructions

## Project Overview

This is an embedded image viewer for ESP32-P4 with DSI displays, targeting AllSky camera displays. The architecture uses hardware-accelerated image processing, flicker-free double-buffering, captive portal WiFi setup, web-based configuration, and full Home Assistant MQTT integration.

**Recent Major Updates:**
- Captive portal WiFi setup with DNS redirection and auto-configuration
- Clean display initialization (no debug clutter on first boot if WiFi configured)
- Cache misalignment fixes for PPA DMA operations
- Improved first-boot experience with centered WiFi setup instructions

## Critical Architecture Patterns

### Memory Management Strategy

**PSRAM is mandatory** - all image buffers MUST use `ps_malloc()`:
- `imageBuffer` (download buffer): `w * h * 2` bytes
- `fullImageBuffer` (active display): 1MB fixed (512×512 max at RGB565)
- `pendingFullImageBuffer` (decode target): 1MB for flicker-free swaps
- `scaledBuffer` (transforms): `w * h * 8` bytes (4× display size)

**PPA (Picture Processing Accelerator) requires DMA-aligned buffers**:
```cpp
heap_caps_aligned_alloc(64, size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM)
```

**Cache Coherency Critical**: After DMA operations, always align size to 64-byte cache line:
```cpp
size_t srcSizeAligned = (srcSize + 63) & ~63;
esp_cache_msync(ppa_src_buffer, srcSizeAligned, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
```
Failure to align causes cache misalignment errors and system instability.

Critical thresholds in `config.h`:
- Heap: `CRITICAL_HEAP_THRESHOLD` (50KB)
- PSRAM: `CRITICAL_PSRAM_THRESHOLD` (100KB)

### Flicker-Free Image Display (Double Buffering)

The system uses a **pending/active buffer swap** to eliminate flicker:

1. Download → `imageBuffer` (compressed JPEG)
2. Decode → `pendingFullImageBuffer` (RGB565 pixels)
3. Set `imageReadyToDisplay = true` flag
4. Main loop swaps buffers atomically
5. Transform/render from `fullImageBuffer`

**Never write directly to `fullImageBuffer` during decode** - always use the pending buffer and swap.

### Hardware Acceleration (PPA)

ESP32-P4's PPA handles scaling/rotation via `ppa_accelerator.cpp`:
- Supports RGB565 format only
- Requires cache coherency: `esp_cache_msync()` before/after DMA
- Handles 0°/90°/180°/270° rotation in hardware
- Max scale factor ~2× (limited by destination buffer)

**Display pause during operations** prevents LCD underrun:
```cpp
displayManager.pauseDisplay();  // Before heavy memory operations
// ... decode or PPA operation ...
displayManager.resumeDisplay();
```

### Modular Component Architecture

Each subsystem is a singleton class with global instance:
- `displayManager` - DSI panel, brightness, debug overlay
- `configStorage` - NVS persistence (uses Arduino Preferences)
- `wifiManager` - WiFi/AP mode (not NetworkManager despite filename)
- `mqttManager` - MQTT client lifecycle
- `ppaAccelerator` - Hardware image transforms
- `systemMonitor` - Watchdog, heap/PSRAM tracking
- `captivePortal` - First-boot WiFi setup
- `haDiscovery` - Home Assistant MQTT discovery
- `taskRetryHandler` - Async retry logic with exponential backoff
- `webConfig` - Web server for configuration UI

All managers use debug function pointers set in `setup()`:
```cpp
wifiManager.setDebugFunctions(debugPrint, debugPrintf, &firstImageLoaded);
```

**Debug Output Philosophy:**
- Debug messages only shown before first image loads (`firstImageLoaded` flag)
- Clean startup on provisioned devices (no clutter)
- WiFi setup mode shows minimal, centered instructions
- All debug text is centered horizontally for visual appeal
- Debug area scrolls when reaching screen bottom

### Display Configuration System

Two display configs via `CURRENT_SCREEN` in `displays_config.h`:
- `SCREEN_3INCH_4_DSI` (800×800) - Waveshare 3.4"
- `SCREEN_4INCH_DSI` (720×720) - Waveshare 4.0"

Each has vendor-specific LCD init commands and DSI timing parameters. **Do not modify these arrays** unless adding new hardware.

## Developer Workflows

### Build & Upload

**PowerShell script** (recommended): `.\compile-and-upload.ps1 -ComPort COM3`
- Auto-detects Arduino CLI or uses direct ESP32 toolchain
- Patches GFX Library for ESP32-P4 compatibility (MIPI_DSI_PHY_CLK_SRC_DEFAULT fix)
- Uses FQBN: `esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled`

**Arduino IDE**:
1. Board: ESP32-P4-Function-EV-Board
2. Partition: 13MB app / 7MB data (32MB flash)
3. PSRAM: **Enabled** (required)
4. Flash Size: 32MB

**GitHub CI**: `.github/workflows/arduino-compile.yml` runs on PR/push - includes GFX library patch step.

### Required Libraries

Install via Arduino Library Manager:
- GFX Library for Arduino (1.6.3+) - **Needs ESP32-P4 patch in CI/local builds**
- JPEGDEC (1.8.4+)
- PubSubClient (2.8.0+)

### Serial Debugging

9600 baud. Debug messages only appear **before first image loads** (`firstImageLoaded` flag).

**Display Initialization Behavior:**
- **WiFi not provisioned**: Shows centered WiFi setup instructions only
- **WiFi provisioned**: Shows startup banner with system info (heap, PSRAM, dimensions)
- Debug messages cleared after first image loads for clean operation

**Runtime serial commands** (single characters):
- Scale: `+`/`-` (±0.1)
- Move: `WASD` (10px steps)
- Rotate: `QE` (90° steps)
- Navigation: `N` (next image), `R` (refresh), `T` (cycle toggle)
- Reset: `X` (transforms), `C` (config), `F` (factory)
- Info: `I` (status), `M` (memory), `V` (version)
- Brightness: `L`/`K` (±10%)
- Reboot: `B`
- Help: `H`, `?`

### Configuration Hierarchy

1. **Defaults** in `config_storage.cpp::setDefaults()`
2. **Persisted NVS** (loaded at boot)
3. **Web UI changes** (save to NVS immediately)
4. **MQTT commands** (via Home Assistant, save to NVS)

**First boot**: If `config.wifiProvisioned == false`, launches captive portal at `192.168.4.1` (SSID: `AllSky-Display-Setup`).

### Captive Portal WiFi Setup (First Boot)

When `wifiProvisioned == false`, the device automatically enters WiFi setup mode:

1. **Clear display** - Shows only WiFi setup instructions (no debug clutter)
2. **AP Mode** - Creates open WiFi network `AllSky-Display-Setup`
3. **DNS Server** - Redirects all domains to `192.168.4.1` for captive portal
4. **Web Interface** - Network scan, SSID selection, password entry
5. **Credential Save** - Saves to NVS and sets `wifiProvisioned = true`
6. **Auto-reboot** - Reboots after 3 seconds to apply WiFi configuration

Portal implementation in `captive_portal.cpp`:
- Uses `DNSServer` for universal domain redirection
- `WebServer` on port 80 with routes: `/`, `/scan`, `/connect`
- JavaScript-based network list with signal strength indicators
- Matching dark blue theme consistent with main web UI
- 5-minute timeout before continuing without WiFi

**Important**: Always call `esp_task_wdt_reset()` during connection attempts to prevent watchdog resets.

### Web Interface

Express-like routing in `web_config.cpp`:
- `/` - System status dashboard
- `/wifi` - Network settings
- `/mqtt` - MQTT/HA config
- `/images` - Multi-image sources (up to 10)
- `/display` - Brightness, transforms
- `/advanced` - Thresholds, intervals
- `/api/*` - REST endpoints (JSON responses)

Web server runs on port 8080. All pages use a dark blue theme with real-time status updates via `/api/status`.

## Project-Specific Conventions

### Multi-Image Cycling

Config in `config.cpp`:
```cpp
const char* DEFAULT_IMAGE_SOURCES[] = { "http://...", ... };
const int DEFAULT_IMAGE_SOURCE_COUNT = 5;
const bool DEFAULT_CYCLING_ENABLED = true;
```

Each image has **per-source transforms** stored in NVS:
```cpp
config.imageTransforms[index].scaleX/scaleY/offsetX/offsetY/rotation
```

Cycling logic in main loop:
- Check `currentCycleInterval` elapsed
- Call `advanceToNextImage()` (handles random order)
- Update `currentImageIndex` and `currentImageURL`

### Touch Gesture Detection

GT911 capacitive touch controller via I²C:
- **Single tap**: Next image
- **Double tap** (within 400ms): Toggle cycling/single-refresh mode

State machine in `ESP32-P4-Allsky-Display.ino`:
```cpp
enum TouchState { TOUCH_IDLE, TOUCH_PRESSED, TOUCH_RELEASED, TOUCH_WAITING_FOR_SECOND_TAP };
```

Timing constants:
- `TOUCH_DEBOUNCE_MS` (50ms)
- `DOUBLE_TAP_TIMEOUT_MS` (400ms)
- `MIN_TAP_DURATION_MS` (50ms)

### Home Assistant MQTT Discovery

`ha_discovery.cpp` publishes discovery messages for:
- Light entity (display brightness)
- Switch entities (cycling, random order)
- Number entities (intervals, global/per-image transforms)
- Select entity (image source picker)
- Button entities (next, refresh, reset)
- Sensors (WiFi RSSI, heap, PSRAM, uptime)

**Topic structure**:
```
homeassistant/{component}/{device_id}/{entity_id}/config
{ha_state_topic}/{entity}
{ha_state_topic}/command/{entity}
```

Device ID derived from MAC address (lowercase hex, no colons).

### Watchdog & Memory Monitoring

`systemMonitor` wraps ESP32 task watchdog:
```cpp
systemMonitor.forceResetWatchdog();  // Call during long operations
```

**Automatic watchdog resets** every 1 second in main loop. If operation exceeds `IMAGE_PROCESS_TIMEOUT` (5s), system resets processing flag.

Memory checks every 30 seconds log warnings if below thresholds.

### Task Retry Handler

`taskRetryHandler` provides async retry logic with exponential backoff for network operations:
- Processes in main loop via `taskRetryHandler.process()`
- Supports task types: `TASK_NETWORK_CONNECT`, `TASK_MQTT_CONNECT`, `TASK_IMAGE_DOWNLOAD`, `TASK_SYSTEM_INIT`, `TASK_CUSTOM`
- Configurable max attempts, base interval, and exponential backoff
- Task statuses: `TASK_PENDING`, `TASK_RUNNING`, `TASK_SUCCESS`, `TASK_FAILED`, `TASK_RETRYING`, `TASK_CANCELLED`
- Call `taskRetryHandler.addTask()` to register tasks with callback functions
- Prevents blocking operations from hanging the main loop

## Common Integration Points

### Adding New Manager Class

1. Create `new_manager.h/.cpp` with singleton pattern:
   ```cpp
   class NewManager { ... };
   extern NewManager newManager;
   ```
2. Add to `ESP32-P4-Allsky-Display.ino` includes
3. Initialize in `setup()`
4. Set debug functions if needed
5. Call `newManager.update()` in `loop()` if continuous operation required

### Adding Config Parameters

1. Add to `ConfigStruct` in `config_storage.h`
2. Initialize in `setDefaults()`
3. Add NVS save/load in `saveConfig()`/`loadConfig()`
4. Add getter/setter methods
5. Add web UI fields in `web_config_pages.cpp`
6. Add API endpoint in `web_config_api.cpp`
7. (Optional) Add HA entity in `ha_discovery.cpp`

### Adding Display Resolutions

1. Define new `SCREEN_*` constant in `displays_config.h`
2. Add vendor init commands array (consult LCD datasheet)
3. Create `DisplayConfig` struct with DSI timing params
4. Update `#if CURRENT_SCREEN == ...` conditionals
5. Test with actual hardware (timing errors cause blank/garbled screen)

## Known Gotchas

- **GFX Library incompatibility**: CI/builds require patching `Arduino_ESP32DSIPanel.cpp` to replace `MIPI_DSI_PHY_CLK_SRC_DEFAULT` with `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M`
- **Cache alignment critical**: PPA DMA operations must use cache-aligned sizes (64-byte boundaries) or cause alignment errors
- **WiFi stability**: `wifiManager.checkConnection()` called every loop - reconnects automatically on drops
- **Captive portal watchdog**: Must call `esp_task_wdt_reset()` during WiFi connection attempts (up to 10 seconds)
- **MQTT reconnect backoff**: Exponential backoff in `mqttManager` to prevent rapid reconnect storms
- **JPEG decode hangs**: 5-second timeout with watchdog protection; display paused during decode
- **PPA rotation limits**: Only 90° increments supported by hardware; arbitrary angles require software fallback (not implemented)
- **Image size limits**: 512×512 max due to 1MB buffer; larger images truncated silently
- **PSRAM speed**: Use large memcpy chunks (1KB) for network reads to maximize throughput
- **First boot display**: Check `wifiProvisioned` flag before showing debug output to avoid cluttering WiFi setup screen
- **Serial baud rate**: Fixed at 9600 baud throughout project lifecycle

## File Structure Notes

- Root directory contains all `.cpp/.h/.ino` files (flat structure)
- `ESP32-P4-Allsky-Display/` subdirectory is PlatformIO project (ignore for Arduino IDE)
- `displays_config.h` has extensive register tables - treat as read-only data
- `web_config_html.h` is minified HTML/CSS/JS - regenerate from source if editing UI
- `MANUAL_SETUP.md` describes hardcoded WiFi config (deprecated in favor of captive portal)

## Testing Approach

No automated unit tests. Validation via:
1. Arduino CI compilation check
2. Manual testing on hardware with serial monitor
3. GitHub Actions build on PR/push
4. Memory analysis workflow (`memory-analysis.yml`) for binary size tracking

When debugging, check Serial output for:
- `DEBUG:` prefixes (verbose info)
- `WARNING:` (performance/network issues)
- `ERROR:` (failures requiring attention)
