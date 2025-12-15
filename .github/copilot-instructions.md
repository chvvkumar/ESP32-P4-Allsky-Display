# ESP32-P4 AllSky Display - AI Coding Agent Guide

## Project Overview
ESP32-P4 firmware for displaying all-sky camera images with hardware-accelerated transforms. Uses DSI display, PPA (Pixel Processing Accelerator), MQTT/Home Assistant integration, and OTA updates with A/B partitions.

## Architecture

### Core Components (modular .cpp/.h pairs)
- **display_manager**: Arduino_GFX wrapper for DSI panel, brightness control, debug overlay
- **ppa_accelerator**: ESP32-P4 hardware scaling/rotation (PPA engine), DMA-aligned buffers
- **network_manager**: WiFi, NTP, ArduinoOTA, captive portal fallback
- **mqtt_manager**: PubSubClient wrapper, HA auto-discovery, status publishing
- **config_storage**: NVS persistence (Preferences), multi-image sources, transforms
- **web_config**: WebServer (port 8080) + WebSocket console (port 81), ElegantOTA at `/update`
- **crash_logger**: RTC memory + NVS crash dumps, backtrace preservation, NTP timestamps
- **system_monitor**: Watchdog management, memory monitoring, task retry handler
- **captive_portal**: WiFi setup AP mode with QR code, DNS redirects

### Data Flow
1. **Image acquisition**: HTTP download → JPEG decode (JPEGDEC) → RGB565 buffer (`pendingFullImageBuffer`)
2. **Transform pipeline**: PPA scale/rotate → `scaledBuffer` → display framebuffer via `draw16bitRGBBitmap()`
3. **Double buffering**: Download to `pendingFullImageBuffer` while `fullImageBuffer` is displayed (flicker-free)

### Memory Layout (PSRAM critical)
- `imageBuffer`: Download scratch (1x display, ~1.28MB)
- `fullImageBuffer`: Current displayed image (4MB, supports 1448×1448 max)
- `pendingFullImageBuffer`: Next image being prepared (4MB double-buffer)
- `scaledBuffer`: PPA output (4x display = 2.0× max scale, ~5.12MB)
- **Order matters**: Buffers allocated in `setup()` *before* display init to ensure contiguous PSRAM

## Critical Conventions

### Display Configuration
- **Compile-time selection**: `displays_config.h` defines `CURRENT_SCREEN` (3.4" vs 4.0" display)
- **GFX Library patch required**: `Arduino_ESP32DSIPanel.cpp` must change `MIPI_DSI_PHY_CLK_SRC_DEFAULT` to `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M` (ESP-IDF 5.5+ type mismatch fix)
- **Auto-applied by script**: `compile-and-upload.ps1` patches library before compile

### Watchdog Management
- **30-second timeout**: Set via `configStorage.getWatchdogTimeout()` in `setup()`, accommodates slow HTTP downloads
- **RAII pattern**: Use `WATCHDOG_SCOPE()` macro (from `watchdog_scope.h`) in long-running functions - auto-resets on entry/exit
- **Manual resets**: `systemMonitor.forceResetWatchdog()` for loops or operations >30s

### Logging System
- **Always use LOG macros**: `LOG_INFO()`, `LOG_ERROR()`, etc. (not `Serial.print*`) - routes to Serial + WebSocket console
- **Severity levels**: `LOG_DEBUG` (hidden by default) → `LOG_INFO` → `LOG_WARNING` → `LOG_ERROR` → `LOG_CRITICAL`
- **Remote debugging**: WebSocket console at `http://[ip]:8080/console` streams all logs with severity filtering

### Configuration Management
- **Global instance**: `configStorage` (Preferences wrapper), always `saveConfig()` after writes
- **Multi-image support**: Up to 10 URLs in `imageSourceCount` array, cycling controlled by `cyclingEnabled` + `randomOrderEnabled`
- **Per-image transforms**: Use `config_storage.h` structs for scale/offset/rotation per image index

### OTA Safety
- **Partition scheme**: `partitions.csv` defines 10MB app0/app1 (A/B), bootloader handles rollback
- **Never block during OTA**: Display paused, watchdog extended, config preserved in NVS
- **Progress display**: `displayManager.showOTAProgress()` draws to framebuffer without disrupting upload

## Build Workflow

### PowerShell Script (Preferred)
```powershell
# If device is connected via USB (serial available)
.\compile-and-upload.ps1 -ComPort COM3

# If device is NOT connected to serial (compile only, then OTA)
.\compile-and-upload.ps1 -SkipUpload -OutputFolder "C:\Users\Kumar\Desktop\New folder"
```
**Auto-handles**: GFX library patching, `build_info.h` git hash injection, Arduino CLI detection, memory size reporting

**Choose method based on connection:**
- USB connected → Upload directly via serial (faster for first flash)
- Network only → Compile binary, then upload via ElegantOTA at `http://allskyesp32.lan:8080/update`

### Arduino IDE Manual Setup
- **Board**: ESP32-P4-Function-EV-Board
- **PSRAM**: Enabled (required, fail compilation if missing)
- **Partition**: 13MB app / 7MB data (32MB) - uses `partitions.csv` in sketch folder
- **Flash**: 32MB, 921600 baud
- **Apply GFX patch manually** (see `docs/02_installation.md` - Compiling From Source section)

### Git Build Info
- `build_info.h` auto-generated with `GIT_COMMIT_HASH`, `GIT_BRANCH`, `BUILD_DATE` - committed as template, updated at compile time

## Common Patterns

### Adding a New Serial Command
1. Add to `command_interpreter.h` command table
2. Implement handler in `command_interpreter.cpp` 
3. Update help text in `printHelp()`
4. Use `LOG_INFO_F()` for feedback (appears in Serial + WebSocket console)

### Adding Web Config Parameter
1. Add getter/setter to `ConfigStorage` class in `config_storage.h`
2. Add NVS key in `config_storage.cpp` (namespace "config")
3. Add form field in `web_config_pages.cpp` HTML generator
4. Add API handler in `web_config_api.cpp` (JSON endpoint)
5. Save with `configStorage.saveConfig()` - persists across reboots

### Handling Large Images
- **Max supported**: 1448×1448 (4MB buffer = 1448² × 2 bytes RGB565)
- **Scale limit**: `MAX_SCALE = sqrt(SCALED_BUFFER_MULTIPLIER)` (default 2.0× from 4× multiplier)
- **Check before download**: Validate Content-Length header against `FULL_IMAGE_BUFFER_SIZE`

### PPA Hardware Acceleration
- **Source/dest must be DMA-aligned**: `ppa_accelerator.cpp` uses `heap_caps_aligned_alloc()`
- **Rotation angles**: 0°, 90°, 180°, 270° only (hardware limitation)
- **Async operation**: PPA has internal buffers, returns immediately, check status before next call

## Debugging

### Runtime Device Information
**Device URL**: `http://allskyesp32.lan:8080/` (mDNS hostname)
**Runtime API**: `http://allskyesp32.lan:8080/api/info` - JSON endpoint with system state
- Use this API to check memory usage, uptime, WiFi status, config values, image cycle state
- Always check runtime state before making assumptions about issues

### Serial Monitor (9600 baud)
- Comprehensive logs: WiFi status, HTTP headers, JPEG decode progress, memory allocation failures
- Commands: `H` for help, `M` for memory, `I` for system info, `V` for version

### WebSocket Console
- Access at `http://allskyesp32.lan:8080/console`, auto-connects, real-time log streaming with severity badges
- Features: Auto-scroll, message counter, log download, connect/disconnect

### Crash Analysis
- **Backtrace preserved**: Crash address saved to NVS + RTC memory
- **Decode with addr2line**: `xtensa-esp32-elf-addr2line -e build/*.elf -f -p <address>`
- **Web view**: `/console` shows crash section from `crashLogger.dumpCrashLogs()`
- See `docs/06_troubleshooting.md` for full crash diagnosis workflow

### Memory Issues
- **Check PSRAM first**: `ESP.getFreePsram()` should be >100KB (critical threshold)
- **Runtime check**: Query `http://allskyesp32.lan:8080/api/info` for current heap/PSRAM values
- **Largest block**: `heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)` shows fragmentation
- **Buffer order**: If allocation fails in `setup()`, reduce `SCALED_BUFFER_MULTIPLIER` in `config.h`

## Troubleshooting Philosophy
**Always find and fix root causes** - avoid band-aid solutions that mask underlying issues:
- Memory leak? Find the allocation source, don't just increase buffer sizes
- Watchdog timeout? Fix the blocking operation, don't just extend timeout
- Network issues? Check WiFi signal/router logs, don't just retry blindly
- Image won't load? Verify URL accessibility, image format, size constraints - check `/api/info` for error details

## Key Files Reference
- `displays_config.h`: Display vendor init sequences, timing params (800×800 vs 720×720)
- `config.h`: All tunables (timeouts, buffer sizes, MQTT topics, default transforms)
- `partitions.csv`: Flash layout (OTA0/OTA1 must be equal size for bootloader)
- `build_info.h`: Git metadata (auto-generated, shows in web UI version)
- `compile-and-upload.ps1`: One-stop compile/upload/patch script
- `docs/02_installation.md`: Installation, library patching, hardcoded WiFi, advanced config
- `docs/05_ota_updates.md`: ElegantOTA/ArduinoOTA workflows, safety mechanisms

## Testing Checklist
- [ ] Compile with PSRAM enabled (compilation fails otherwise)
- [ ] Test with display size matching `CURRENT_SCREEN` in `displays_config.h`
- [ ] Verify GFX library patch applied (check for `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M`)
- [ ] OTA update with invalid binary triggers rollback (bootloader auto-reverts)
- [ ] WebSocket console shows logs with severity filtering
- [ ] Watchdog doesn't trigger during slow image downloads (30s timeout)
- [ ] Multi-image cycling respects random order setting
