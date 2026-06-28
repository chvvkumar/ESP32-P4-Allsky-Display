<!-- refreshed: 2026-06-28 -->
# Codebase Structure

**Analysis Date:** 2026-06-28

## Directory Layout

```
ESP32-P4-Allsky-Display/
├── ESP32-P4-Allsky-Display.ino  # Main Arduino sketch (entry point, global state)
├── build_info.h                 # Compile-time build metadata
├── config.h                     # Constants, log severity enum, buffer size macros
├── config.cpp                   # (config init shim)
├── config_storage.cpp/.h        # NVS Preferences-backed config singleton
├── config_backup.cpp/.h         # JSON config export/import with schema versioning
├── logging.h                    # LOG_INFO / LOG_ERROR / LOG_DEBUG_F macros
├── crash_logger.cpp/.h          # RTC+NVS crash ring buffer
├── system_monitor.cpp/.h        # Watchdog management, heap/PSRAM health checks
├── watchdog_scope.h             # RAII watchdog reset scope guard
├── task_retry_handler.cpp/.h    # Background failure retry scheduler
├── command_interpreter.cpp/.h   # Serial command parser
├── device_health.cpp/.h         # Health metrics aggregation
├── display_manager.cpp/.h       # DSI panel init, framebuffer draw, brightness
├── displays_config.cpp/.h       # DSI timing configs for 3.4" and 4.0" panels
├── ppa_accelerator.cpp/.h       # ESP32-P4 PPA hardware scale+rotate
├── image_utils.cpp/.h           # Software bilinear scale/rotate, color temp
├── image_presets.cpp/.h         # Named preset image source list
├── stb_image.c/.h               # Single-file STB image decoder (moon texture)
├── moon_ephemeris.c/.h          # Astronomical phase/illumination computation (C)
├── moon_equirect_data.h         # Embedded equirectangular moon texture JPEG data
├── moon_sphere.cpp/.h           # tgx 3D sphere renderer for moon display
├── moon_interaction.c/.h        # Touch drag-to-rotate state machine (C)
├── network_manager.cpp/.h       # WiFi STA/AP, roaming, HTTP Date time-sync, OTA init
├── mqtt_manager.cpp/.h          # PubSubClient MQTT broker, HA discovery, heartbeat
├── ha_rest_client.cpp/.h        # Home Assistant REST brightness polling (Core 0 task)
├── ha_discovery.cpp/.h          # HA MQTT auto-discovery message builder
├── captive_portal.cpp/.h        # First-boot WiFi provisioning AP + captive portal
├── ota_manager.cpp/.h           # ElegantOTA firmware update integration
├── web_config.cpp/.h            # WebServer + WebSocket class, route registration
├── web_config_api.cpp           # REST API handler implementations
├── web_config_pages.cpp         # HTML page generation functions
├── web_config_html.h            # PROGMEM CSS/JS/static HTML assets
├── gt911.cpp/.h                 # GT911 capacitive touch IC driver
├── touch.cpp/.h                 # ESP-IDF LCD touch abstraction layer
├── i2c.cpp/.h                   # I2C bus init and helpers
├── wifi_qr_code.h               # Embedded QR code JPEG (WiFi provisioning)
├── partitions.csv               # Custom partition table
├── compile-and-upload.ps1       # PowerShell build/flash helper script
├── .planning/
│   └── codebase/                # GSD codebase analysis documents
├── .github/
│   ├── workflows/               # CI: compile check, badge update
│   └── agents/                  # GitHub AI agent config
├── docs/
│   ├── developer/               # Developer docs
│   ├── plans/                   # Implementation plans
│   └── archive/                 # Archived specs
├── test/                        # (placeholder, no test files currently)
├── tools/                       # Utility scripts
├── firmware_esphosted/          # ESP32-C6 co-processor (ESP-Hosted) firmware binaries
├── images/                      # Project images/screenshots
├── logs/                        # Local build/debug logs
└── build/                       # Arduino IDE build output (gitignored)
```

## File Groupings by Subsystem

### Main Sketch (Entry Point)

| File | Purpose |
|------|---------|
| `ESP32-P4-Allsky-Display.ino` | `setup()`, `loop()`, all global variables, `downloadAndDisplayImage()`, `renderFullImage()`, `renderMoonToPendingBuffer()`, `serviceMoonDrag()`, `downloadTask()`, touch FSM, image cycling logic, JPEG callbacks |

### Display and Rendering

| File | Purpose |
|------|---------|
| `display_manager.cpp` / `display_manager.h` | DSI panel lifecycle, `drawBitmap()`, brightness via LEDC PWM, debug text overlay, OTA progress screen, pause/resume for PSRAM contention |
| `displays_config.cpp` / `displays_config.h` | `DisplayConfig` struct, DSI timing tables for 3.4" (800x800) and 4.0" (1448x1448) Waveshare panels, runtime panel selection |
| `ppa_accelerator.cpp` / `ppa_accelerator.h` | ESP32-P4 Pixel Processing Accelerator: `scaleRotateImageZeroCopy()`, DMA-aligned buffer management, hardware availability check |
| `image_utils.cpp` / `image_utils.h` | `softwareTransform()` bilinear scale+rotate fallback, `adjustColorTemperature()` RGB565 color shift |
| `image_presets.cpp` / `image_presets.h` | Named preset URL lists (GOES, NOAA, AllSky sources) |

### Image Acquisition

| File | Purpose |
|------|---------|
| `ESP32-P4-Allsky-Display.ino` | `downloadAndDisplayImage()`: `HTTPClient` GET, chunked stream into `imageBuffer`, JPEGDEC decode into `pendingFullImageBuffer` |
| `stb_image.c` / `stb_image.h` | STB image library used exclusively for decoding the lunar equirectangular JPEG texture |

### Moon Renderer

| File | Purpose |
|------|---------|
| `moon_sphere.cpp` / `moon_sphere.h` | tgx 3D sphere tessellation and rendering, texture mapping, starfield/glow background, `moon_sphere_render()`, `moon_sphere_render_into()`, disk-scale control |
| `moon_ephemeris.c` / `moon_ephemeris.h` | `moon_compute()` C function: phase name, illumination fraction, waxing/waning, libration |
| `moon_interaction.c` / `moon_interaction.h` | Drag state machine: `moon_drag_move()`, `moon_drag_end()`, `moon_drag_advance()`, free-spin and snap-back logic |
| `moon_equirect_data.h` | Embedded 2048x1024 equirectangular lunar surface JPEG (raw byte array) |

### Networking

| File | Purpose |
|------|---------|
| `network_manager.cpp` / `network_manager.h` | `WiFiManager`: STA connect/reconnect with exponential backoff, AP mode, mesh roaming, HTTP Date header time-sync (replaces broken SNTP), ArduinoOTA init, WiFi scan |
| `mqtt_manager.cpp` / `mqtt_manager.h` | `MQTTManager`: PubSubClient connect, HA MQTT auto-discovery, heartbeat/availability publish, sensor data, message callback |
| `ha_rest_client.cpp` / `ha_rest_client.h` | Home Assistant REST API client: polls brightness lux sensor, adjusts display brightness accordingly (runs on Core 0) |
| `ha_discovery.cpp` / `ha_discovery.h` | Builds and publishes HA MQTT discovery payloads |
| `captive_portal.cpp` / `captive_portal.h` | First-boot AP + DNS + simple HTTP form for WiFi credential entry |
| `ota_manager.cpp` / `ota_manager.h` | ElegantOTA integration, OTA progress callbacks to display |

### Web Configuration Layer

| File | Purpose |
|------|---------|
| `web_config.h` | `WebConfig` class declaration: `WebServer*`, `WebSocketsServer*`, all handler method prototypes, HTML generator prototypes, utility function prototypes, `extern WebConfig webConfig` |
| `web_config.cpp` | `WebConfig::begin()` (route registration for ~40 routes), `handleClient()`, `loopWebSocket()`, `broadcastLog()`, chunked response helpers (`beginChunkedHtmlResponse` / `endChunkedHtmlResponse`), utility functions (`formatUptime`, `escapeHtml`, `escapeJson`) |
| `web_config_api.cpp` | All REST API handler implementations: `handleSaveConfig()`, `handleAddImageSource()`, `handleRemoveImageSource()`, `handleUpdateImageTransform()`, `handleSelectImage()`, `handleTuneImage()`, `handleBackup()`, `handleRestore()`, `handleScreenshot()` (hardware JPEG encode), `handleGetHealth()`, `handleWiFiScan()`, and ~25 others. Directly accesses globals from main sketch via `extern`. |
| `web_config_pages.cpp` | HTML page generators: `generateMainPage()`, `generateNetworkPage()`, `generateConsolePage()`, `generateMQTTPage()`, `generateImagePage()`, `generateDisplayPage()`, `generateAdvancedPage()`, `generateStatusPage()`, `generateSerialCommandsPage()`, `generateAPIReferencePage()`. Returns Arduino `String` objects built by concatenation with `html.reserve(N)` pre-allocation. |
| `web_config_html.h` | PROGMEM string constants: `HTML_CSS[]`, JavaScript blocks, navigation HTML, header/footer fragments. Loaded into flash, copied to RAM only when sending responses. |

### Persistence and Configuration

| File | Purpose |
|------|---------|
| `config.h` | Global constants, `LogSeverity` enum, buffer size macros, timing macros, `CONFIG_SCHEMA_VERSION` |
| `config.cpp` | Configuration initialization (`initializeConfiguration()` called from `setup()`) |
| `config_storage.cpp` / `config_storage.h` | `ConfigStorage` class: NVS Preferences wrapper with dirty-bitmask grouped writes (`DIRTY_WIFI`, `DIRTY_MQTT`, etc.), `ConfigLock` RAII mutex, getters/setters for all config fields including per-image transforms and source list |
| `config_backup.cpp` / `config_backup.h` | JSON serialize/deserialize for `/api/backup` (GET) and `/api/restore` (POST), schema version migration hook |

### Infrastructure / Cross-cutting

| File | Purpose |
|------|---------|
| `logging.h` | `LOG_INFO()`, `LOG_ERROR()`, `LOG_DEBUG_F()`, `LOG_CRITICAL()` etc. macros; delegates to `logPrint()` / `logPrintf()` in main sketch |
| `system_monitor.cpp` / `system_monitor.h` | Task WDT init/config, `forceResetWatchdog()`, heap/PSRAM threshold monitoring, `isSystemHealthy()` |
| `watchdog_scope.h` | RAII `WatchdogScope` to reset WDT on scope entry/exit |
| `crash_logger.cpp` / `crash_logger.h` | RTC memory ring buffer + NVS fallback for crash messages surviving reboot; `wasLastBootCrash()`, `saveBeforeReboot()` |
| `task_retry_handler.cpp` / `task_retry_handler.h` | Retry scheduler for network/MQTT/image failures |
| `command_interpreter.cpp` / `command_interpreter.h` | Serial command parser for runtime control (set URL, force refresh, toggle cycling, etc.) |
| `device_health.cpp` / `device_health.h` | Aggregates heap, PSRAM, WiFi RSSI, uptime for `/api/health` endpoint |
| `build_info.h` | `BUILD_DATE`, `BUILD_TIME`, firmware version string |

### Touch / Input

| File | Purpose |
|------|---------|
| `gt911.cpp` / `gt911.h` | GT911 I2C touch IC driver: init, register read, coordinate reporting |
| `touch.cpp` / `touch.h` | ESP-IDF `esp_lcd_touch` abstraction: `esp_lcd_touch_handle_t`, `esp_lcd_touch_get_coordinates()` |
| `i2c.cpp` / `i2c.h` | I2C bus init, pin config for GT911 communication |

## Key File Locations

**Entry Point:**
- `ESP32-P4-Allsky-Display.ino`: `setup()` at line 515, `loop()` at line 2113

**Route Registration:**
- `web_config.cpp`: `WebConfig::begin()` registers all HTTP routes, lines ~17-110

**REST API Handlers:**
- `web_config_api.cpp`: all `WebConfig::handle*()` implementations

**HTML Pages:**
- `web_config_pages.cpp`: `generateMainPage()`, `generateNetworkPage()`, etc.

**Static Web Assets:**
- `web_config_html.h`: PROGMEM `HTML_CSS[]`, JavaScript, nav fragments

**Config Getters/Setters:**
- `config_storage.h`: complete API surface for all persistent settings

**Buffer Allocation:**
- `ESP32-P4-Allsky-Display.ino`: lines 568-675 (`setup()` buffer pre-allocation block)

**Partition Table:**
- `partitions.csv`: custom flash layout

**DSI Panel Configs:**
- `displays_config.cpp`: timing structs for both supported panel sizes

## Naming Conventions

**Files:**
- `snake_case.cpp` / `snake_case.h` for all modules (e.g., `display_manager.cpp`, `config_storage.h`)
- Main sketch uses project name: `ESP32-P4-Allsky-Display.ino`
- Web config split by concern: `web_config.cpp`, `web_config_api.cpp`, `web_config_pages.cpp`, `web_config_html.h`
- Moon subsystem: `moon_` prefix for all related files

**Classes:**
- `PascalCase` (e.g., `DisplayManager`, `ConfigStorage`, `WebConfig`, `PPAAccelerator`)

**Global instances:**
- `camelCase` singleton pattern: `displayManager`, `configStorage`, `webConfig`, `mqttManager`, `wifiManager`, `systemMonitor`, `crashLogger`

**Constants / Macros:**
- `UPPER_SNAKE_CASE` for all `#define` constants and dirty-bit flags

## Where to Add New Code

**New REST API endpoint:**
- Add handler declaration to `web_config.h` (private method of `WebConfig`)
- Register route in `WebConfig::begin()` inside `web_config.cpp`
- Implement handler body in `web_config_api.cpp`
- If it needs to trigger main sketch behavior, add an `extern` for the relevant global or add a callback mechanism

**New config field:**
- Add getter/setter to `config_storage.h` and implement in `config_storage.cpp`
- Add `DIRTY_` bitmask constant if a new group is needed, or reuse existing group
- Update `config_backup.cpp` if the field should be included in backup/restore

**New HTML config page:**
- Add page generator `generateXxxPage()` to `web_config_pages.cpp`
- Add route handler in `web_config.h` and `web_config.cpp`
- Add navigation entry in `generateNavigation()` in `web_config.cpp`
- Add large static JS/CSS to `web_config_html.h` as PROGMEM

**New display panel support:**
- Add `DisplayConfig` struct instance and init command array in `displays_config.cpp`
- Add `#define SCREEN_Xxx` constant and conditional in `displays_config.h`
- Update runtime selection logic in `display_manager.cpp`

**New networking integration:**
- Create `new_service.cpp` / `new_service.h` following the `begin()` / `update()` pattern
- Add global instance, call `begin()` in `setup()`, call `update()` in `loop()`

## Special Directories

**`firmware_esphosted/`:**
- Purpose: Pre-built ESP32-C6 co-processor firmware (ESP-Hosted SDIO WiFi bridge)
- Generated: External (Espressif ESP-Hosted project)
- Committed: Yes (binary blobs for flashing C6 co-processor)

**`.planning/codebase/`:**
- Purpose: GSD codebase analysis documents for AI-assisted planning
- Generated: Yes (by `/gsd-map-codebase`)
- Committed: No (in `.gitignore`)

**`build/`:**
- Purpose: Arduino IDE compilation output
- Generated: Yes
- Committed: No

**`test/`:**
- Purpose: Placeholder for future tests
- Generated: No
- Committed: Yes (empty)

---

*Structure analysis: 2026-06-28*
