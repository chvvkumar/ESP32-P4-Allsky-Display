<!-- refreshed: 2026-06-28 -->
# Architecture

**Analysis Date:** 2026-06-28

## System Overview

```text
┌──────────────────────────────────────────────────────────────────┐
│              ESP32-P4-Allsky-Display.ino (main sketch)           │
│  setup() / loop() / downloadAndDisplayImage() / renderFullImage()│
│  global state: buffers, cycling vars, touch state, image flags   │
└───┬───────────────┬────────────────┬──────────────┬─────────────┘
    │               │                │              │
    ▼               ▼                ▼              ▼
┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌──────────────┐
│ Display  │  │ Image    │  │  Networking  │  │  Web Config  │
│  Layer   │  │ Pipeline │  │    Layer     │  │    Layer     │
│          │  │          │  │              │  │              │
│display_  │  │JPEGDEC   │  │network_      │  │web_config.cpp│
│manager   │  │stb_image │  │manager       │  │web_config_   │
│ppa_      │  │image_    │  │mqtt_manager  │  │  api.cpp     │
│accelera- │  │utils     │  │ha_rest_      │  │web_config_   │
│tor       │  │image_    │  │  client      │  │  pages.cpp   │
│displays_ │  │presets   │  │captive_      │  │web_config_   │
│config    │  │moon_     │  │  portal      │  │  html.h      │
│          │  │sphere    │  │ota_manager   │  │              │
└──────────┘  └──────────┘  └──────────────┘  └──────────────┘
    │               │                │
    ▼               ▼                ▼
┌──────────────────────────────────────────────────────────────────┐
│                     Persistence / Config Layer                    │
│  config_storage.cpp  config.h  config_backup.cpp                 │
│  NVS (ESP32 Preferences)  crash_logger.cpp                       │
└──────────────────────────────────────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────────────────────────────────────┐
│                Cross-cutting / Infrastructure                     │
│  system_monitor  watchdog_scope  task_retry_handler              │
│  command_interpreter  logging.h  build_info.h                    │
│  touch.cpp / gt911.cpp / i2c.cpp                                 │
└──────────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File(s) |
|-----------|----------------|---------|
| Main sketch | setup/loop orchestration, global buffer management, image fetch/decode coordination, touch gesture FSM, cycling logic | `ESP32-P4-Allsky-Display.ino` |
| DisplayManager | DSI panel init, framebuffer draw, brightness, debug overlay, OTA screen | `display_manager.cpp` / `display_manager.h` |
| PPAAccelerator | ESP32-P4 hardware PPA scale+rotate, DMA buffer management | `ppa_accelerator.cpp` / `ppa_accelerator.h` |
| DisplaysConfig | DSI panel timing parameters for 3.4" and 4.0" Waveshare panels | `displays_config.cpp` / `displays_config.h` |
| ImageUtils | Software bilinear scale/rotate fallback, color temperature adjustment | `image_utils.cpp` / `image_utils.h` |
| ImagePresets | Named preset image source definitions | `image_presets.cpp` / `image_presets.h` |
| MoonSphere | tgx-based 3D moon sphere renderer, texture decode, disk-scale control | `moon_sphere.cpp` / `moon_sphere.h` |
| MoonEphemeris | Astronomical phase/illumination/libration computation | `moon_ephemeris.c` / `moon_ephemeris.h` |
| MoonInteraction | Touch drag-to-rotate state machine for interactive moon | `moon_interaction.c` / `moon_interaction.h` |
| WiFiManager / NetworkManager | WiFi STA connect, AP mode, roaming, HTTP Date time-sync, OTA init | `network_manager.cpp` / `network_manager.h` |
| MQTTManager | PubSubClient broker connection, HA discovery, heartbeat, command messages | `mqtt_manager.cpp` / `mqtt_manager.h` |
| HARestClient | Home Assistant REST API brightness reading (Core 0 task) | `ha_rest_client.cpp` / `ha_rest_client.h` |
| HADiscovery | Home Assistant MQTT auto-discovery message publication | `ha_discovery.cpp` / `ha_discovery.h` |
| CaptivePortal | First-boot WiFi provisioning AP + captive portal web form | `captive_portal.cpp` / `captive_portal.h` |
| OTAManager | ElegantOTA firmware update integration | `ota_manager.cpp` / `ota_manager.h` |
| WebConfig | Arduino WebServer + WebSocketsServer HTTP/WS handler, route registration | `web_config.cpp` / `web_config.h` |
| WebConfigAPI | REST API handler implementations (POST/GET endpoints) | `web_config_api.cpp` |
| WebConfigPages | HTML page generation functions (in-memory string building) | `web_config_pages.cpp` |
| WebConfigHTML | PROGMEM CSS/JS/static HTML asset strings | `web_config_html.h` |
| ConfigStorage | NVS-backed persistent config (Preferences), dirty-bit grouped writes, mutex-protected | `config_storage.cpp` / `config_storage.h` |
| ConfigBackup | JSON serialization/deserialization for config export/import with schema versioning | `config_backup.cpp` / `config_backup.h` |
| SystemMonitor | Task watchdog management, heap/PSRAM threshold checks | `system_monitor.cpp` / `system_monitor.h` |
| CrashLogger | RTC+NVS crash log ring buffer, survives reboot | `crash_logger.cpp` / `crash_logger.h` |
| TaskRetryHandler | Background retry scheduling for network/MQTT/image failures | `task_retry_handler.cpp` / `task_retry_handler.h` |
| CommandInterpreter | Serial command parsing for runtime control | `command_interpreter.cpp` / `command_interpreter.h` |
| GT911 / Touch / I2C | GT911 capacitive touchscreen driver via I2C | `gt911.cpp`, `touch.cpp`, `i2c.cpp` |
| DeviceHealth | System health metrics aggregation | `device_health.cpp` / `device_health.h` |

## Pattern Overview

**Overall:** Single-process Arduino sketch with FreeRTOS dual-core task split.

**Key Characteristics:**
- Core 1 runs `setup()` / `loop()` (display, rendering, touch, web server polling)
- Core 0 runs `downloadTask` (HTTP fetch via `imageDownloadPending` flag) and `haRestClient` task
- Double-buffer pattern: `pendingFullImageBuffer` decoded on Core 0, swapped to `fullImageBuffer` on Core 1 under mutex
- All persistent state in global variables declared in `ESP32-P4-Allsky-Display.ino`
- Modules expose singleton global instances (`displayManager`, `webConfig`, `configStorage`, etc.)
- Arduino `WebServer` is polled synchronously in `loop()` via `webConfig.handleClient()`

## Layers

**Main Orchestration:**
- Purpose: Entry point, global state, image fetch/decode dispatch, render trigger, touch FSM, cycling scheduler
- Location: `ESP32-P4-Allsky-Display.ino`
- Contains: `setup()`, `loop()`, `downloadAndDisplayImage()`, `renderFullImage()`, `renderMoonToPendingBuffer()`, `serviceMoonDrag()`, `downloadTask()`, all global image/cycling/touch variables
- Depends on: all subsystem modules
- Used by: nothing (top of tree)

**Display / Rendering Layer:**
- Purpose: DSI panel control, pixel write, hardware accelerated scale/rotate, software fallback
- Location: `display_manager.cpp`, `ppa_accelerator.cpp`, `displays_config.cpp`, `image_utils.cpp`
- Contains: `Arduino_ESP32DSIPanel`, `Arduino_DSI_Display` GFX objects, PPA client handle, bilinear scaler
- Depends on: `displays_config.h`, `config_storage` (for display type selection at runtime)
- Used by: main sketch, `web_config_api.cpp` (screenshot endpoint)

**Image Acquisition / Moon Pipeline:**
- Purpose: HTTP JPEG download, JPEGDEC decode into pending buffer, moon sphere render
- Location: `ESP32-P4-Allsky-Display.ino` (`downloadAndDisplayImage`, `JPEGDraw` callback), `moon_sphere.cpp`, `moon_ephemeris.c`, `moon_interaction.c`
- Contains: `HTTPClient`, `JPEGDEC jpeg`, stb_image decode for moon texture, tgx sphere renderer
- Depends on: `network_manager`, `config_storage`, `ppa_accelerator`
- Used by: main sketch (triggered by `imageDownloadPending` flag)

**Networking Layer:**
- Purpose: WiFi STA management, time sync via HTTP Date header, MQTT broker, HA REST
- Location: `network_manager.cpp`, `mqtt_manager.cpp`, `ha_rest_client.cpp`, `ha_discovery.cpp`, `captive_portal.cpp`, `ota_manager.cpp`
- Contains: `WiFiManager` class, `MQTTManager` (PubSubClient), `HARestClient`, `ElegantOTA`
- Depends on: `config_storage`
- Used by: main sketch (via `update()` calls in loop)

**Web Configuration Layer:**
- Purpose: HTTP server for browser UI, REST API for config/control, WebSocket log console
- Location: `web_config.cpp`, `web_config_api.cpp`, `web_config_pages.cpp`, `web_config_html.h`
- Contains: `WebServer` (Arduino), `WebSocketsServer`, `ElegantOTA`, ~40 route handlers, HTML page generators, PROGMEM CSS/JS
- Depends on: `config_storage`, `display_manager`, `mqtt_manager`, `network_manager`, `ota_manager`, `device_health`, `ha_rest_client`, `ha_discovery`, `config_backup`, `crash_logger`
- Used by: main sketch (`webConfig.handleClient()` and `webConfig.loopWebSocket()` in loop)

**Persistence / Config Layer:**
- Purpose: NVS-backed config storage, JSON backup/restore, crash log persistence
- Location: `config_storage.cpp`, `config_backup.cpp`, `crash_logger.cpp`
- Contains: `Preferences` NVS wrapper, dirty-bit bitmask group writes, `ConfigLock` RAII mutex, schema versioning
- Depends on: nothing except Arduino/IDF
- Used by: nearly all modules

**Infrastructure / Cross-cutting:**
- Purpose: Watchdog management, health checks, retry scheduling, serial commands, logging
- Location: `system_monitor.cpp`, `watchdog_scope.h`, `task_retry_handler.cpp`, `command_interpreter.cpp`, `logging.h`, `build_info.h`
- Depends on: `config_storage`
- Used by: all modules

## Data Flow

### Primary Image Display Path

1. `loop()` detects `shouldUpdate` (time elapsed or `lastUpdate==0`) and sets `imageDownloadPending = true` (`ESP32-P4-Allsky-Display.ino:2416`)
2. `downloadTask()` on Core 0 picks up flag and calls `downloadAndDisplayImage()` (`ESP32-P4-Allsky-Display.ino:2096`)
3. `downloadAndDisplayImage()` calls `HTTPClient::GET()`, streams JPEG bytes into `imageBuffer` (PSRAM, `esp32-P4-Allsky-Display.ino:~1400-1700`)
4. `JPEGDEC::openRAM()` / `decode()` with `JPEGDraw` callback writes RGB565 pixels into `pendingFullImageBuffer` (`ESP32-P4-Allsky-Display.ino:260-270`)
5. `imageReadyToDisplay` atomic flag set to `true` under `imageBufferMutex` (`ESP32-P4-Allsky-Display.ino:1858`)
6. `loop()` on Core 1 detects `imageReadyToDisplay`, takes mutex, swaps `pendingFullImageBuffer` and `fullImageBuffer` pointers (`ESP32-P4-Allsky-Display.ino:2436-2447`)
7. `renderFullImage()` called: checks scale/rotation, uses PPA hardware (`ppaAccelerator.scaleRotateImageZeroCopy`) or software fallback (`ImageUtils::softwareTransform`) into `scaledBuffer` (`ESP32-P4-Allsky-Display.ino:1041-1143`)
8. `displayManager.drawBitmap()` writes RGB565 to DSI framebuffer (`display_manager.cpp`)
9. DSI panel auto-flush (configured via `DisplayConfig::auto_flush`) pushes framebuffer to LCD hardware

### Moon Render Path

1. `getCurrentImageURL()` returns `"moon://"` prefix URL
2. `downloadAndDisplayImage()` routes to `renderMoonToPendingBuffer()` (`ESP32-P4-Allsky-Display.ino:1354`)
3. `moon_compute()` calculates phase/illumination/libration from system time (`moon_ephemeris.c`)
4. `moon_sphere_render()` renders RGB565 sphere into PSRAM-allocated frame, writes to `pendingFullImageBuffer` (`moon_sphere.cpp`)
5. Same buffer-swap and display path as above (steps 6-9)

### Interactive Moon Drag Path

1. `updateTouchState()` detects drag gesture exceeding `MOON_DRAG_THRESHOLD_PX` on a moon source
2. Sets `interactiveMoonMode = true`, `loop()` calls `serviceMoonDrag()` which blocks
3. `serviceMoonDrag()` renders at 240x240, PPA-upscales to panel, repeats until settled (`ESP32-P4-Allsky-Display.ino:1256-1312`)

### Web API Request Path

1. `loop()` calls `webConfig.handleClient()` synchronously on Core 1
2. `WebServer::handleClient()` (Arduino library) dispatches to registered lambda/method handler
3. Handler in `web_config_api.cpp` reads args, calls `configStorage` setters, may set `lastUpdate=0` to force image refresh
4. Response sent via `server->send()` (blocking on Core 1)

**State Management:**
- All image cycling state is global in `ESP32-P4-Allsky-Display.ino` (mutable, no encapsulation)
- Config state is encapsulated in `ConfigStorage` singleton with mutex
- Inter-task communication uses `imageDownloadPending` (volatile bool), `imageReadyToDisplay` (std::atomic<bool>), and `imageBufferMutex` (FreeRTOS semaphore)

## Key Abstractions

**ConfigStorage:**
- Purpose: Single source of truth for all runtime configuration, NVS-persisted
- Examples: `config_storage.cpp`, `config_storage.h`
- Pattern: Singleton global instance `configStorage`, dirty-bitmask grouped NVS writes, RAII `ConfigLock` for thread safety

**WebConfig class:**
- Purpose: Encapsulates all Arduino WebServer route setup, HTML generation, WebSocket log broadcasting
- Examples: `web_config.h`, `web_config.cpp`, `web_config_api.cpp`, `web_config_pages.cpp`
- Pattern: Single class instance `webConfig`, methods split across three `.cpp` files

**Double-buffer (pending/active):**
- Purpose: Decode into `pendingFullImageBuffer` on Core 0 while `fullImageBuffer` is displayed on Core 1, swap atomically
- Pattern: Pointer swap under `imageBufferMutex`, `std::atomic<bool> imageReadyToDisplay` as ready flag

## Entry Points

**`setup()`:**
- Location: `ESP32-P4-Allsky-Display.ino:515`
- Triggers: Power-on / reboot
- Responsibilities: Disable bootloader WDT, init crash logger, init config, init system monitor, decode moon texture, pre-allocate all PSRAM buffers, init display, init PPA, init networking, start web server on port 8080, init touch, create FreeRTOS `downloadTask` on Core 0

**`loop()`:**
- Location: `ESP32-P4-Allsky-Display.ino:2113`
- Triggers: Continuous Arduino loop on Core 1
- Responsibilities: Feed watchdog, handle captive portal (WiFi setup mode), poll `webConfig.handleClient()`, poll `webConfig.loopWebSocket()`, update all subsystem modules, process serial commands, update touch state, check image cycling timer, trigger async download, swap display buffers when `imageReadyToDisplay`

**`downloadTask()` (FreeRTOS, Core 0):**
- Location: `ESP32-P4-Allsky-Display.ino:2078`
- Triggers: `imageDownloadPending = true` set by `loop()`
- Responsibilities: Call `downloadAndDisplayImage()`, clear `imageDownloadPending`

## Architectural Constraints

- **Threading:** Dual-core FreeRTOS. Core 1: Arduino loop (display, web, touch). Core 0: download task, HA REST client task. `imageBufferMutex` protects buffer swap.
- **Global state:** Extensive global variables in `ESP32-P4-Allsky-Display.ino` (buffers, cycling state, touch state). These are not encapsulated. Any new module must access them via `extern` declarations.
- **WebServer polling:** `WebServer::handleClient()` is called twice per `loop()` iteration (lines ~2165 and ~2215). It is synchronous and blocking. Long API handlers (e.g., OTA, restore) block the display loop for their duration.
- **Memory:** All pixel buffers allocated in PSRAM via `ps_malloc` / `heap_caps_aligned_alloc`. Order of allocation in `setup()` is critical: moon texture decode and image buffers before display init ensures contiguous PSRAM blocks.
- **Time sync:** SNTP/UDP does not function on ESP32-P4 with ESP-Hosted. Wall clock is set from HTTP `Date` response headers only (`network_manager.cpp`).
- **No circular imports:** Each module includes only `config.h`, `config_storage.h`, `logging.h` as common headers.

## Web Layer Details (Migration Target)

The entire web layer is implemented on top of the **Arduino `WebServer` library** and **`WebSocketsServer`** (arduinoWebSockets). Key facts for a migration to ESP-IDF `esp_http_server`:

- **Route registration:** All ~40 routes registered in `WebConfig::begin()` inside `web_config.cpp:17-100` using `server->on(path, method, lambda)` pattern.
- **Handler split:** Route handler declarations in `web_config.h`, HTML page generators in `web_config_pages.cpp`, REST API handlers in `web_config_api.cpp`.
- **Static assets:** CSS/JS stored as `PROGMEM` string literals in `web_config_html.h`. Page HTML is built dynamically as Arduino `String` objects in `web_config_pages.cpp` (in-memory string concatenation, pre-reserved with `html.reserve(N)`).
- **Chunked response:** `beginChunkedHtmlResponse()` / `endChunkedHtmlResponse()` helpers exist in `web_config.cpp` to split large HTML pages into pieces to reduce peak heap. These call `server->sendContent()`.
- **WebSocket:** `WebSocketsServer wsServer` (port 81) used for live log streaming to browser console. `webConfig.broadcastLog()` called from `logPrint()` / `debugPrint()` throughout the codebase.
- **OTA:** `ElegantOTA` integrated via `otaManager`; hooks into the same `WebServer` instance.
- **Screenshot endpoint:** `/api/screenshot` (GET) in `web_config_api.cpp` uses ESP32-P4 hardware JPEG encoder (`driver/jpeg_encode.h`) to encode the current framebuffer and stream it.
- **Global coupling:** `web_config_api.cpp` directly reads/writes many globals from `ESP32-P4-Allsky-Display.ino` via `extern` declarations (`lastUpdate`, `cyclingEnabled`, `currentImageIndex`, `imageDownloadPending`, etc.). This is the primary coupling that complicates a clean web layer migration.

## Anti-Patterns

### Global extern coupling between web API and main sketch

**What happens:** `web_config_api.cpp` declares `extern` for ~15 globals from `ESP32-P4-Allsky-Display.ino` (e.g., `lastUpdate`, `cyclingEnabled`, `imageDownloadPending`, `scaledBuffer`, `fullImageBuffer`).

**Why it's wrong:** The web layer cannot be moved or tested independently; any refactor of global variable names or types in the main sketch breaks the API handlers.

**Do this instead:** Expose a thin control interface (callbacks or a struct of function pointers) from the main sketch that the web layer calls to trigger actions, rather than touching globals directly.

### WebServer::handleClient() called twice per loop

**What happens:** `webConfig.handleClient()` appears at both `ESP32-P4-Allsky-Display.ino:2165` and `ESP32-P4-Allsky-Display.ino:2215` within the same `loop()` iteration.

**Why it's wrong:** Redundant calls with logging overhead; the second call inside the `isRunning()` block is the primary one, the first is a fast-path attempt that also resets the web server on failure.

**Do this instead:** With `esp_http_server`, the server runs its own task; `handleClient()` polling is eliminated entirely.

## Error Handling

**Strategy:** Watchdog-reset-heavy defensive programming. Most failure paths log to Serial and crash logger, then either continue (non-fatal) or call `ESP.restart()` (fatal memory allocation failures).

**Patterns:**
- Buffer allocation failures in `setup()` call `crashLogger.saveBeforeReboot()` then `ESP.restart()`
- Download failures set `imageDownloadFailed = true` and trigger retry via `taskRetryHandler`
- WebSocket broadcast: fire-and-forget, no error checked
- MQTT reconnect uses exponential backoff in `MQTTManager`

## Cross-Cutting Concerns

**Logging:** `logPrint()` / `logPrintf()` in `ESP32-P4-Allsky-Display.ino` routes to Serial, `crashLogger`, and `webConfig.broadcastLog()`. `LOG_INFO_F()` / `LOG_ERROR_F()` macros in `logging.h` wrap these.

**Validation:** None at API layer; `web_config_api.cpp` reads args with `server->arg()` and passes directly to `configStorage` setters.

**Authentication:** None. Web UI and REST API are open on the LAN. No session management.

---

*Architecture analysis: 2026-06-28*
