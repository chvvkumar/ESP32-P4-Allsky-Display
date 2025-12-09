# ESP32-P4 AllSky Display - API Reference

## Table of Contents
- [DisplayManager](#displaymanager)
- [PPAAccelerator](#ppaaccelerator)
- [NetworkManager](#networkmanager)
- [MQTTManager](#mqttmanager)
- [WebConfig](#webconfig)
- [ConfigStorage](#configstorage)
- [SystemMonitor](#systemmonitor)
- [CrashLogger](#crashlogger)
- [HADiscovery](#hadiscovery)
- [OTAManager](#otamanager)

---

## DisplayManager

**File:** `display_manager.h`, `display_manager.cpp`

### Class Overview

```cpp
/**
 * @class DisplayManager
 * @brief Manages the MIPI DSI display interface and rendering operations.
 * 
 * This class encapsulates the Arduino_GFX library for controlling the ESP32-P4's
 * MIPI DSI display panel. It provides brightness control, debug overlay text,
 * display pause/resume for memory bandwidth management, and OTA progress display.
 * 
 * @note The display must be initialized AFTER all PSRAM buffers are allocated
 *       to ensure sufficient contiguous memory for the framebuffer (~1.28MB).
 */
class DisplayManager;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Initialize the display hardware and allocate framebuffer.
 * 
 * Configures the MIPI DSI panel based on CURRENT_SCREEN define from displays_config.h.
 * Allocates ~1.28MB framebuffer in PSRAM for 800×800 displays (or 720×720).
 * 
 * @return true if initialization successful, false if display init or memory allocation failed
 * 
 * @warning MUST be called AFTER allocating image buffers in setup() to prevent fragmentation.
 * @note This is a heavy operation that takes 2-3 seconds and requires watchdog resets.
 * 
 * Example:
 * @code
 * // In setup() - allocate buffers FIRST
 * fullImageBuffer = (uint16_t*)ps_malloc(FULL_IMAGE_BUFFER_SIZE);
 * pendingFullImageBuffer = (uint16_t*)ps_malloc(FULL_IMAGE_BUFFER_SIZE);
 * // THEN initialize display
 * if (!displayManager.begin()) {
 *     Serial.println("Display init failed!");
 * }
 * @endcode
 */
bool begin();
```

#### getGFX()

```cpp
/**
 * @brief Get direct access to the underlying Arduino_GFX display object.
 * 
 * Provides low-level access for custom drawing operations not covered by
 * DisplayManager's high-level API.
 * 
 * @return Pointer to Arduino_DSI_Display object, or nullptr if not initialized
 * 
 * @note Use with caution - direct GFX calls bypass DisplayManager's state tracking.
 * 
 * Example:
 * @code
 * Arduino_DSI_Display* gfx = displayManager.getGFX();
 * if (gfx) {
 *     gfx->fillRect(10, 10, 100, 50, COLOR_BLUE);
 *     gfx->flush();  // Always flush after direct drawing
 * }
 * @endcode
 */
Arduino_DSI_Display* getGFX() const;
```

#### getWidth() / getHeight()

```cpp
/**
 * @brief Get display width in pixels.
 * @return Display width (800 for 3.4"/4.0" displays, 720 for some panels)
 */
int16_t getWidth() const;

/**
 * @brief Get display height in pixels.
 * @return Display height (800 for 3.4"/4.0" displays, 720 for some panels)
 */
int16_t getHeight() const;
```

#### setBrightness()

```cpp
/**
 * @brief Set display backlight brightness.
 * 
 * Controls the PWM duty cycle of the LCD_BL_PWM signal (GPIO26).
 * Uses LEDC channel 0 with 10-bit resolution (0-1023 internal range).
 * 
 * @param brightness Brightness percentage (0-100)
 *                   - 0: Backlight off (display still active)
 *                   - 100: Maximum brightness (1023 PWM duty)
 * 
 * @note Brightness is clamped to 0-100 range if out of bounds.
 * @note Setting is persistent across reboots if saved via ConfigStorage.
 * 
 * Example:
 * @code
 * displayManager.setBrightness(50);  // Set to 50% brightness
 * configStorage.setDefaultBrightness(50);
 * configStorage.saveConfig();  // Persist across reboots
 * @endcode
 */
void setBrightness(int brightness);
```

#### getBrightness()

```cpp
/**
 * @brief Get current backlight brightness.
 * @return Current brightness percentage (0-100)
 */
int getBrightness() const;
```

#### debugPrint() / debugPrintf()

```cpp
/**
 * @brief Print debug message to display overlay.
 * 
 * Displays colored text on the screen during startup and initialization.
 * Automatically scrolls when reaching MAX_DEBUG_LINES (15 lines).
 * After firstImageLoaded, debug messages no longer appear on screen
 * (but still go to Serial and WebSocket console).
 * 
 * @param message Text to display (null-terminated C string)
 * @param color RGB565 color value (default: COLOR_WHITE)
 *              Use predefined colors: COLOR_RED, COLOR_GREEN, COLOR_YELLOW, etc.
 * 
 * @note Debug output is automatically disabled after first image loads.
 * @note Messages are also logged to Serial and WebSocket console.
 * 
 * Example:
 * @code
 * displayManager.debugPrint("WiFi connected", COLOR_GREEN);
 * displayManager.debugPrint("Error loading image", COLOR_RED);
 * @endcode
 */
void debugPrint(const char* message, uint16_t color = COLOR_WHITE);

/**
 * @brief Print formatted debug message to display overlay.
 * 
 * Printf-style formatted output to the debug overlay.
 * Supports standard printf format specifiers.
 * 
 * @param color RGB565 color value for text
 * @param format Printf-style format string
 * @param ... Variable arguments matching format specifiers
 * 
 * @note Maximum message length: 256 characters
 * 
 * Example:
 * @code
 * displayManager.debugPrintf(COLOR_CYAN, "IP: %s", WiFi.localIP().toString().c_str());
 * displayManager.debugPrintf(COLOR_YELLOW, "Free heap: %d bytes", ESP.getFreeHeap());
 * @endcode
 */
void debugPrintf(uint16_t color, const char* format, ...);
```

#### clearScreen()

```cpp
/**
 * @brief Clear entire display to specified color.
 * 
 * Fills the entire framebuffer with a solid color. This is a blocking operation
 * that takes ~50-100ms for 800×800 displays.
 * 
 * @param color RGB565 color value (default: COLOR_BLACK)
 * 
 * @warning Clears all content including any displayed images.
 * @note Use sparingly - frequent clears cause visible flicker.
 * 
 * Example:
 * @code
 * displayManager.clearScreen();              // Clear to black
 * displayManager.clearScreen(COLOR_WHITE);   // Clear to white
 * @endcode
 */
void clearScreen(uint16_t color = COLOR_BLACK);
```

#### drawBitmap()

```cpp
/**
 * @brief Draw RGB565 bitmap to display at specified position.
 * 
 * Performs DMA-based memory copy from source buffer to framebuffer.
 * This is the primary method for rendering images to the screen.
 * 
 * @param x X coordinate (top-left corner)
 * @param y Y coordinate (top-left corner)
 * @param bitmap Pointer to RGB565 pixel data (16-bit per pixel)
 * @param w Width of bitmap in pixels
 * @param h Height of bitmap in pixels
 * 
 * @note Coordinates and dimensions are NOT bounds-checked - caller must ensure valid region.
 * @note Does not automatically flush - call gfx->flush() if immediate display needed.
 * @note Bitmap data must remain valid until flush completes.
 * 
 * Example:
 * @code
 * // Draw 512×512 image centered on 800×800 display
 * int16_t x = (800 - 512) / 2;
 * int16_t y = (800 - 512) / 2;
 * displayManager.drawBitmap(x, y, imageBuffer, 512, 512);
 * @endcode
 */
void drawBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h);
```

#### pauseDisplay() / resumeDisplay()

```cpp
/**
 * @brief Temporarily pause display refresh to prevent memory bandwidth conflicts.
 * 
 * During heavy PPA operations or JPEG decoding, the display's continuous refresh
 * can cause DMA contention and LCD underrun errors (visible as screen corruption).
 * Pausing stops the refresh temporarily to free up memory bandwidth.
 * 
 * @warning Always call resumeDisplay() after pause, even on error paths.
 * @note Pause duration should be minimized (<500ms) to avoid visible screen freeze.
 * 
 * Example:
 * @code
 * displayManager.pauseDisplay();
 * bool success = ppaAccelerator.scaleImage(src, srcW, srcH, dst, dstW, dstH);
 * displayManager.resumeDisplay();  // Always resume, even on error
 * @endcode
 */
void pauseDisplay();

/**
 * @brief Resume display refresh after pauseDisplay().
 * 
 * Re-enables the display refresh that was stopped by pauseDisplay().
 */
void resumeDisplay();
```

#### showOTAProgress()

```cpp
/**
 * @brief Display OTA update progress screen.
 * 
 * Shows a progress bar and status message during firmware updates.
 * Only redraws when progress changes to minimize flash usage.
 * 
 * @param title Title text (e.g., "Firmware Update")
 * @param percent Progress percentage (0-100)
 * @param message Optional status message (default: nullptr shows "Updating...")
 * 
 * @note Screen remains visible until next clearScreen() or drawBitmap() call.
 * @note Progress bar is color-coded: yellow (0-99%), green (100%)
 * 
 * Example:
 * @code
 * void onOTAProgress(unsigned int progress, unsigned int total) {
 *     uint8_t percent = (progress * 100) / total;
 *     displayManager.showOTAProgress("OTA Update", percent);
 * }
 * @endcode
 */
void showOTAProgress(const char* title, uint8_t percent, const char* message = nullptr);
```

#### setFirstImageLoaded()

```cpp
/**
 * @brief Set flag indicating first image has been successfully loaded.
 * 
 * Disables debug overlay text after first successful image display.
 * Prevents debug messages from obscuring the displayed image.
 * 
 * @param loaded true to mark first image as loaded, false to reset
 * 
 * @note Called automatically by main sketch after first JPEG decode succeeds.
 */
void setFirstImageLoaded(bool loaded);
```

---

## PPAAccelerator

**File:** `ppa_accelerator.h`, `ppa_accelerator.cpp`

### Class Overview

```cpp
/**
 * @class PPAAccelerator
 * @brief ESP32-P4 hardware image processing using Pixel Processing Accelerator.
 * 
 * The PPA is a dedicated hardware block in the ESP32-P4 SoC for real-time image
 * transformations. It performs scaling and rotation operations via DMA, bypassing
 * the CPU for significantly faster performance (10-50× speedup vs. software).
 * 
 * @note All buffers must be DMA-aligned (64-byte boundary) and allocated in PSRAM.
 * @note Only supports RGB565 color format (native display format).
 * @note Rotation is limited to 0°, 90°, 180°, 270° (hardware angles only).
 */
class PPAAccelerator;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Initialize PPA hardware and allocate DMA-aligned buffers.
 * 
 * Allocates two DMA-aligned buffers in PSRAM:
 * - Source buffer: FULL_IMAGE_BUFFER_SIZE (4MB)
 * - Destination buffer: displayWidth × displayHeight × SCALED_BUFFER_MULTIPLIER × 2
 * 
 * @param displayWidth Width of the display in pixels (e.g., 800)
 * @param displayHeight Height of the display in pixels (e.g., 800)
 * @return true if PPA initialized successfully, false if allocation failed
 * 
 * @note Buffers are aligned to 64-byte boundary (DMA requirement).
 * @note If allocation fails, PPA operations will fall back to software rendering.
 * 
 * Example:
 * @code
 * if (!ppaAccelerator.begin(800, 800)) {
 *     Serial.println("PPA init failed - using software rendering");
 * }
 * @endcode
 */
bool begin(int16_t displayWidth, int16_t displayHeight);
```

#### isAvailable()

```cpp
/**
 * @brief Check if PPA hardware acceleration is available.
 * 
 * Returns false if:
 * - PPA initialization failed
 * - DMA buffer allocation failed
 * - ESP32-P4 hardware doesn't support PPA (wrong chip variant)
 * 
 * @return true if PPA can be used, false if software fallback required
 * 
 * Example:
 * @code
 * if (ppaAccelerator.isAvailable()) {
 *     // Use hardware acceleration
 *     ppaAccelerator.scaleImage(...);
 * } else {
 *     // Use software fallback
 *     softwareScale(...);
 * }
 * @endcode
 */
bool isAvailable() const;
```

#### scaleRotateImage()

```cpp
/**
 * @brief Scale and rotate image using PPA hardware acceleration.
 * 
 * Performs simultaneous scaling and rotation in a single hardware pass.
 * This is the most efficient operation for transform pipelines.
 * 
 * @param srcPixels Source image buffer (RGB565 format)
 * @param srcWidth Source image width in pixels
 * @param srcHeight Source image height in pixels
 * @param dstPixels Destination buffer (RGB565 format, must be large enough)
 * @param dstWidth Desired output width in pixels
 * @param dstHeight Desired output height in pixels
 * @param rotation Rotation angle (0.0, 90.0, 180.0, or 270.0 degrees)
 * @return true if operation succeeded, false if parameters invalid or hardware error
 * 
 * @warning Source and destination buffers must not overlap.
 * @warning Destination buffer size must be ≥ dstWidth × dstHeight × 2 bytes.
 * @warning Rotation must be exactly 0, 90, 180, or 270 (hardware limitation).
 * 
 * @note Operation is asynchronous - PPA works independently while CPU continues.
 * @note Typical performance: 512×512 → 800×800 + 90° rotation in ~180ms.
 * 
 * Example:
 * @code
 * // Scale from 512×512 to 800×800 and rotate 90° clockwise
 * if (ppaAccelerator.scaleRotateImage(
 *         fullImageBuffer, 512, 512,
 *         scaledBuffer, 800, 800,
 *         90.0)) {
 *     displayManager.drawBitmap(0, 0, scaledBuffer, 800, 800);
 * }
 * @endcode
 */
bool scaleRotateImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                     uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight,
                     float rotation = 0.0);
```

#### scaleImage()

```cpp
/**
 * @brief Scale image without rotation using PPA hardware.
 * 
 * Specialized version for scaling-only operations (no rotation).
 * Slightly faster than scaleRotateImage() when rotation is not needed.
 * 
 * @param srcPixels Source image buffer (RGB565 format)
 * @param srcWidth Source image width in pixels
 * @param srcHeight Source image height in pixels
 * @param dstPixels Destination buffer (RGB565 format)
 * @param dstWidth Desired output width in pixels
 * @param dstHeight Desired output height in pixels
 * @return true if operation succeeded, false if parameters invalid or hardware error
 * 
 * @note All other constraints from scaleRotateImage() apply.
 * 
 * Example:
 * @code
 * // Downscale QR code from 256×256 to 166×166
 * if (ppaAccelerator.scaleImage(qrBuffer, 256, 256, scaledQR, 166, 166)) {
 *     displayManager.drawBitmap(317, 50, scaledQR, 166, 166);
 * }
 * @endcode
 */
bool scaleImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
               uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight);
```

#### Memory Management

```cpp
/**
 * @brief Get size of PPA source buffer.
 * @return Size in bytes of the internal DMA-aligned source buffer
 */
size_t getSourceBufferSize() const;

/**
 * @brief Get size of PPA destination buffer.
 * @return Size in bytes of the internal DMA-aligned destination buffer
 */
size_t getDestinationBufferSize() const;
```

---

## NetworkManager

**File:** `network_manager.h`, `network_manager.cpp`

### Class Overview

```cpp
/**
 * @class NetworkManager
 * @brief Manages WiFi connectivity and network time synchronization.
 * 
 * Handles WiFi connection with retry backoff, NTP time sync, and ArduinoOTA.
 * Monitors connection health and provides diagnostics.
 */
class NetworkManager;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Initialize WiFi hardware and load credentials from NVS.
 * 
 * Configures WiFi mode, hostname, and prepares for connection.
 * Does NOT connect automatically - call connectToWiFi() afterwards.
 * 
 * @return true if WiFi hardware initialized, false on failure
 * 
 * @warning Must delay 500ms after begin() before calling connectToWiFi()
 *          to allow MAC address initialization (prevents 00:00:00:00:00:00).
 * 
 * Example:
 * @code
 * if (networkManager.begin()) {
 *     delay(500);  // CRITICAL: Allow hardware init
 *     networkManager.connectToWiFi();
 * }
 * @endcode
 */
bool begin();
```

#### connectToWiFi()

```cpp
/**
 * @brief Attempt to connect to WiFi using stored credentials.
 * 
 * Tries up to WIFI_MAX_ATTEMPTS (15) times with exponential backoff.
 * Blocks until connected or maximum attempts reached.
 * 
 * @note Automatically calls syncNTPTime() on successful connection.
 * @note Requires watchdog resets during connection attempts.
 * 
 * Example:
 * @code
 * networkManager.connectToWiFi();
 * if (networkManager.isConnected()) {
 *     Serial.println("WiFi connected!");
 * }
 * @endcode
 */
void connectToWiFi();
```

#### isConnected()

```cpp
/**
 * @brief Check if WiFi is currently connected.
 * @return true if connected to WiFi network, false otherwise
 */
bool isConnected() const;
```

#### syncNTPTime()

```cpp
/**
 * @brief Synchronize system time with NTP server.
 * 
 * Connects to configured NTP server (default: pool.ntp.org) and sets
 * the ESP32 internal RTC. Used for crash log timestamps and MQTT messages.
 * 
 * @note Timezone is configurable via ConfigStorage (default: UTC0).
 * @note Time is persistent across soft resets (RTC continues running).
 * 
 * Example:
 * @code
 * networkManager.syncNTPTime();
 * time_t now = time(nullptr);
 * Serial.println(ctime(&now));  // Prints: "Mon Dec 09 19:30:45 2025"
 * @endcode
 */
void syncNTPTime();
```

#### initOTA() / handleOTA()

```cpp
/**
 * @brief Initialize ArduinoOTA for network firmware updates.
 * 
 * Configures OTA callbacks and starts listening on port 3232.
 * Called automatically after WiFi connection in setup().
 */
void initOTA();

/**
 * @brief Handle incoming OTA requests.
 * 
 * Must be called in main loop to process ArduinoOTA packets.
 * 
 * @note Also checks for connection loss and triggers reconnect if needed.
 */
void handleOTA();
```

#### Network Diagnostics

```cpp
/**
 * @brief Get current IP address.
 * @return IP address as string (e.g., "192.168.1.100"), or "0.0.0.0" if not connected
 */
String getIPAddress() const;

/**
 * @brief Get MAC address.
 * @return MAC address as string (e.g., "AA:BB:CC:DD:EE:FF")
 */
String getMACAddress() const;

/**
 * @brief Get WiFi signal strength.
 * @return RSSI value in dBm (e.g., -65 is stronger than -75)
 *         Returns 0 if not connected.
 */
int getSignalStrength() const;
```

---

## MQTTManager

**File:** `mqtt_manager.h`, `mqtt_manager.cpp`

### Class Overview

```cpp
/**
 * @class MQTTManager
 * @brief MQTT client for Home Assistant integration.
 * 
 * Manages MQTT connection, command subscriptions, and state publishing.
 * Works with HADiscovery class for Home Assistant auto-discovery.
 */
class MQTTManager;
```

### Public Methods

#### begin() / connect()

```cpp
/**
 * @brief Initialize MQTT client with configured broker settings.
 * 
 * Loads MQTT server, port, credentials from ConfigStorage.
 * Does NOT connect automatically - call connect() afterwards.
 * 
 * @return true if configuration loaded, false if settings invalid
 */
bool begin();

/**
 * @brief Connect to MQTT broker.
 * 
 * Attempts connection using configured credentials. If connection fails,
 * sets up exponential backoff for automatic reconnect attempts.
 * 
 * @note Publishes availability (online) and Home Assistant discovery on connect.
 */
void connect();
```

#### isConnected() / loop()

```cpp
/**
 * @brief Check if MQTT client is connected to broker.
 * @return true if connected, false otherwise
 */
bool isConnected();

/**
 * @brief Process incoming MQTT messages.
 * 
 * Must be called frequently (in main loop) to handle subscriptions
 * and maintain connection keepalive.
 */
void loop();
```

#### publishAvailabilityHeartbeat()

```cpp
/**
 * @brief Publish availability status for Home Assistant.
 * 
 * Sends "online" to availability topic, indicating device is responsive.
 * Home Assistant uses this to show device status in UI.
 * 
 * @note Called automatically every 60 seconds by update().
 */
void publishAvailabilityHeartbeat();
```

#### getClient()

```cpp
/**
 * @brief Get raw PubSubClient for manual MQTT operations.
 * 
 * Provides direct access to underlying MQTT client for advanced use cases.
 * 
 * @return Pointer to PubSubClient instance
 * 
 * Example:
 * @code
 * mqttManager.getClient()->publish("custom/topic", "hello");
 * @endcode
 */
PubSubClient* getClient();
```

---

## WebConfig

**File:** `web_config.h`, `web_config.cpp`

### Class Overview

```cpp
/**
 * @class WebConfig
 * @brief Web server for configuration UI and REST API.
 * 
 * Provides HTML configuration pages, REST API endpoints, and WebSocket
 * console for remote management and diagnostics.
 */
class WebConfig;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Start web server on specified port.
 * 
 * Initializes HTTP server, WebSocket server, and ElegantOTA.
 * Registers all route handlers and API endpoints.
 * 
 * @param port HTTP port number (default: 80, but typically use 8080)
 * @return true if server started successfully, false on error
 * 
 * Example:
 * @code
 * if (webConfig.begin(8080)) {
 *     Serial.println("Web server: http://allskyesp32.lan:8080");
 * }
 * @endcode
 */
bool begin(int port = 80);
```

#### handleClient()

```cpp
/**
 * @brief Process incoming HTTP requests.
 * 
 * Must be called frequently (in main loop) to handle web client connections.
 * Processes route requests, API calls, and file downloads.
 * 
 * @note Does not block - returns immediately if no requests pending.
 */
void handleClient();
```

#### loopWebSocket()

```cpp
/**
 * @brief Process WebSocket events.
 * 
 * Handles WebSocket connections, disconnections, and incoming messages.
 * Must be called in main loop for real-time console updates.
 */
void loopWebSocket();
```

#### broadcastLog()

```cpp
/**
 * @brief Broadcast log message to all connected WebSocket clients.
 * 
 * Sends timestamped and severity-tagged messages to web console.
 * Clients can filter by severity level (DEBUG/INFO/WARNING/ERROR/CRITICAL).
 * 
 * @param message Log message text
 * @param color RGB565 color for display (unused by WebSocket, kept for compatibility)
 * @param severity Log severity level (LOG_DEBUG, LOG_INFO, etc.)
 * 
 * @note Messages below client's configured severity are not sent.
 * @note Automatically adds timestamp and severity prefix.
 * 
 * Example:
 * @code
 * webConfig.broadcastLog("Image downloaded successfully", COLOR_GREEN, LOG_INFO);
 * webConfig.broadcastLog("Memory low: 45KB free", COLOR_RED, LOG_WARNING);
 * @endcode
 */
void broadcastLog(const char* message, uint16_t color = 0xFFFF, LogSeverity severity = LOG_INFO);
```

#### isRunning() / isOTAInProgress()

```cpp
/**
 * @brief Check if web server is running.
 * @return true if server is active, false if stopped
 */
bool isRunning();

/**
 * @brief Check if OTA update is in progress.
 * @return true during firmware upload, false otherwise
 * 
 * @note Used to pause image downloads during OTA to prevent conflicts.
 */
bool isOTAInProgress() const;
```

---

## ConfigStorage

**File:** `config_storage.h`, `config_storage.cpp`

### Class Overview

```cpp
/**
 * @class ConfigStorage
 * @brief NVS-based persistent configuration storage.
 * 
 * Stores all runtime-configurable settings in ESP32's Non-Volatile Storage (NVS).
 * Settings persist across reboots and power cycles.
 */
class ConfigStorage;
```

### Public Methods

#### begin() / loadConfig() / saveConfig()

```cpp
/**
 * @brief Initialize NVS storage system.
 * 
 * Opens NVS namespace "config" and prepares for read/write operations.
 * 
 * @return true if NVS initialized, false if flash error
 */
bool begin();

/**
 * @brief Load all configuration from NVS.
 * 
 * Reads stored values or applies defaults if keys don't exist.
 * Called automatically during begin().
 */
void loadConfig();

/**
 * @brief Save all current configuration to NVS.
 * 
 * Writes all settings to flash storage. Must be called after any
 * configuration changes to persist them across reboots.
 * 
 * @warning Flash writes have limited endurance (~100,000 cycles per sector).
 *          Avoid calling excessively (e.g., in tight loops).
 * 
 * Example:
 * @code
 * configStorage.setImageURL("https://example.com/image.jpg");
 * configStorage.setCyclingEnabled(true);
 * configStorage.saveConfig();  // Persist to flash
 * @endcode
 */
void saveConfig();
```

#### WiFi Configuration

```cpp
/**
 * @brief Set WiFi SSID.
 * @param ssid WiFi network name (max 32 characters)
 */
void setWiFiSSID(const String& ssid);

/**
 * @brief Set WiFi password.
 * @param password WiFi password (max 64 characters)
 */
void setWiFiPassword(const String& password);

/**
 * @brief Get WiFi SSID.
 * @return Stored SSID, or empty string if not set
 */
String getWiFiSSID();

/**
 * @brief Get WiFi password.
 * @return Stored password, or empty string if not set
 */
String getWiFiPassword();

/**
 * @brief Check if WiFi credentials have been configured.
 * @return true if SSID is set, false if first boot or after factory reset
 */
bool isWiFiProvisioned();
```

#### Multi-Image Configuration

```cpp
/**
 * @brief Enable or disable multi-image cycling.
 * @param enabled true to cycle through multiple images, false for single image mode
 */
void setCyclingEnabled(bool enabled);

/**
 * @brief Get cycling enabled status.
 * @return true if cycling active, false otherwise
 */
bool getCyclingEnabled();

/**
 * @brief Set image cycling interval.
 * @param interval Time in milliseconds between image switches (10000-3600000)
 */
void setCycleInterval(unsigned long interval);

/**
 * @brief Get cycling interval.
 * @return Interval in milliseconds
 */
unsigned long getCycleInterval();

/**
 * @brief Set image source URL at specific index.
 * @param index Image index (0-9)
 * @param url Full image URL (JPEG format)
 */
void setImageSource(int index, const String& url);

/**
 * @brief Get image source URL at index.
 * @param index Image index (0-9)
 * @return Image URL, or empty string if index invalid
 */
String getImageSource(int index);

/**
 * @brief Add new image source to end of list.
 * @param url Image URL
 * 
 * @note Maximum 10 sources (MAX_IMAGE_SOURCES).
 * @note Does nothing if already at maximum.
 */
void addImageSource(const String& url);

/**
 * @brief Remove image source at index.
 * @param index Image index to remove (0-9)
 * @return true if removed, false if index invalid
 * 
 * @note Shifts remaining sources down to fill gap.
 */
bool removeImageSource(int index);

/**
 * @brief Clear all image sources.
 * 
 * Resets image source count to 0 and clears all URLs.
 */
void clearImageSources();
```

#### Per-Image Transforms

```cpp
/**
 * @brief Set scale factor for specific image.
 * @param index Image index (0-9)
 * @param scale Scale factor (0.1 to MAX_SCALE, typically 2.0)
 */
void setImageScaleX(int index, float scale);
void setImageScaleY(int index, float scale);

/**
 * @brief Get scale factor for specific image.
 * @param index Image index (0-9)
 * @return Scale factor, or default if index invalid
 */
float getImageScaleX(int index);
float getImageScaleY(int index);

/**
 * @brief Set offset for specific image.
 * @param index Image index (0-9)
 * @param offset Offset in pixels (-400 to +400 for 800×800 display)
 */
void setImageOffsetX(int index, int offset);
void setImageOffsetY(int index, int offset);

/**
 * @brief Get offset for specific image.
 * @param index Image index (0-9)
 * @return Offset in pixels
 */
int getImageOffsetX(int index);
int getImageOffsetY(int index);

/**
 * @brief Set rotation angle for specific image.
 * @param index Image index (0-9)
 * @param rotation Rotation in degrees (0, 90, 180, or 270)
 */
void setImageRotation(int index, float rotation);

/**
 * @brief Get rotation angle for specific image.
 * @param index Image index (0-9)
 * @return Rotation in degrees
 */
float getImageRotation(int index);

/**
 * @brief Copy default transform settings to specific image.
 * @param index Image index (0-9)
 * 
 * Applies DEFAULT_SCALE_X/Y, DEFAULT_OFFSET_X/Y, DEFAULT_ROTATION to the image.
 */
void copyDefaultsToImageTransform(int index);
```

---

## SystemMonitor

**File:** `system_monitor.h`, `system_monitor.cpp`

### Class Overview

```cpp
/**
 * @class SystemMonitor
 * @brief Watchdog management and system health monitoring.
 * 
 * Provides watchdog timer control, memory monitoring, and system health checks.
 */
class SystemMonitor;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Initialize watchdog timer with specified timeout.
 * 
 * Configures ESP32 task watchdog with custom timeout to accommodate
 * slow operations like HTTP downloads and JPEG decoding.
 * 
 * @param timeoutMs Watchdog timeout in milliseconds (default: 30000)
 * @return true if watchdog initialized, false on error
 * 
 * @warning If not reset within timeout, device will reboot.
 * 
 * Example:
 * @code
 * // 30-second timeout for slow image downloads
 * systemMonitor.begin(30000);
 * @endcode
 */
bool begin(unsigned long timeoutMs = WATCHDOG_TIMEOUT_MS);
```

#### resetWatchdog() / forceResetWatchdog()

```cpp
/**
 * @brief Reset watchdog timer (automatic in update() loop).
 * 
 * Called automatically by update() every second. Use forceResetWatchdog()
 * for manual resets in long-running functions.
 */
void resetWatchdog();

/**
 * @brief Manually reset watchdog timer immediately.
 * 
 * Use in critical sections or long-running operations to prevent timeout.
 * 
 * @note Should be called every 1-5 seconds during heavy operations.
 * 
 * Example:
 * @code
 * for (int i = 0; i < 1000; i++) {
 *     performExpensiveOperation();
 *     if (i % 10 == 0) {
 *         systemMonitor.forceResetWatchdog();  // Reset every 10 iterations
 *     }
 * }
 * @endcode
 */
void forceResetWatchdog();
```

#### Memory Monitoring

```cpp
/**
 * @brief Get current free heap memory.
 * @return Free heap in bytes
 */
size_t getCurrentFreeHeap() const;

/**
 * @brief Get current free PSRAM.
 * @return Free PSRAM in bytes
 */
size_t getCurrentFreePsram() const;

/**
 * @brief Get minimum free heap since boot.
 * @return Minimum heap in bytes (lowest point reached)
 * 
 * @note Useful for detecting memory leaks (value should stabilize after startup).
 */
size_t getMinFreeHeap() const;

/**
 * @brief Get minimum free PSRAM since boot.
 * @return Minimum PSRAM in bytes
 */
size_t getMinFreePsram() const;
```

#### System Health

```cpp
/**
 * @brief Check system health (memory thresholds).
 * 
 * Compares current heap/PSRAM against critical thresholds:
 * - CRITICAL_HEAP_THRESHOLD (50KB default)
 * - CRITICAL_PSRAM_THRESHOLD (100KB default)
 * 
 * Logs warnings if below thresholds and sets systemHealthy flag.
 * 
 * @note Called automatically every 30 seconds by update().
 */
void checkSystemHealth();

/**
 * @brief Get system health status.
 * @return true if memory above critical thresholds, false if low memory
 */
bool isSystemHealthy() const;

/**
 * @brief Print current memory status to Serial.
 * 
 * Displays current and minimum heap/PSRAM values.
 */
void printMemoryStatus();
```

---

## CrashLogger

**File:** `crash_logger.h`, `crash_logger.cpp`

### Class Overview

```cpp
/**
 * @class CrashLogger
 * @brief Crash detection and log preservation across reboots.
 * 
 * Stores logs in three layers:
 * 1. RTC memory (4KB, survives soft reset)
 * 2. RAM buffer (8KB, current session only)
 * 3. NVS flash (4KB, survives power loss)
 */
class CrashLogger;
```

### Public Methods

#### begin()

```cpp
/**
 * @brief Initialize crash logger and check for previous crashes.
 * 
 * Increments boot counter, checks crash marker in RTC memory,
 * and prepares logging buffers.
 * 
 * @note Call this FIRST in setup() to capture all boot messages.
 * 
 * Example:
 * @code
 * void setup() {
 *     Serial.begin(9600);
 *     crashLogger.begin();  // Initialize before anything else
 *     
 *     if (crashLogger.wasLastBootCrash()) {
 *         Serial.println("Previous boot ended in crash!");
 *     }
 * }
 * @endcode
 */
void begin();
```

#### log() / logf()

```cpp
/**
 * @brief Log message to all buffers (RTC + RAM).
 * @param message Null-terminated C string
 * 
 * @note Messages are also visible in web console if WebSocket connected.
 */
void log(const char* message);

/**
 * @brief Log formatted message (printf-style).
 * @param format Printf format string
 * @param ... Variable arguments
 * 
 * Example:
 * @code
 * crashLogger.logf("Free heap: %d bytes\n", ESP.getFreeHeap());
 * @endcode
 */
void logf(const char* format, ...);
```

#### Crash Detection

```cpp
/**
 * @brief Mark that system is about to crash.
 * 
 * Sets crash marker in RTC memory and saves logs to NVS.
 * Should be called in panic handler or exception callback.
 * 
 * @warning Only call in actual crash conditions - misuse will
 *          trigger false positives on next boot.
 */
void markCrash();

/**
 * @brief Check if last boot was due to crash.
 * @return true if crash marker set, false for normal boot
 */
bool wasLastBootCrash();

/**
 * @brief Get total boot count since NVS clear.
 * @return Number of times device has booted
 */
uint32_t getBootCount();
```

#### Log Retrieval

```cpp
/**
 * @brief Get recent logs from all sources.
 * @param maxBytes Maximum bytes to return (default: 4096)
 * @return Combined logs as String
 * 
 * @note Returns most recent logs first (newest at top).
 */
String getRecentLogs(size_t maxBytes = 4096);

/**
 * @brief Get logs from RTC memory only.
 * @return RTC logs (survives soft reset only)
 */
String getRTCLogs();

/**
 * @brief Get logs from current RAM buffer.
 * @return Current session logs (cleared on reboot)
 */
String getRAMLogs();

/**
 * @brief Get logs from NVS flash.
 * @return Preserved logs from previous boots
 */
String getNVSLogs();
```

#### saveToNVS()

```cpp
/**
 * @brief Save current logs to NVS flash.
 * 
 * Writes current RAM buffer to NVS for preservation across power cycles.
 * 
 * @warning Flash writes are slow (~200ms) and have limited endurance.
 *          Don't call in tight loops.
 * 
 * @note Automatically called on:
 *       - Crash detection (markCrash)
 *       - Intentional reboot (saveBeforeReboot)
 *       - Manual trigger via web UI
 */
void saveToNVS();

/**
 * @brief Save logs before intentional reboot.
 * 
 * Marks this as a clean reboot (not a crash) and saves logs.
 * 
 * Example:
 * @code
 * crashLogger.saveBeforeReboot();
 * delay(100);  // Allow NVS write to complete
 * ESP.restart();
 * @endcode
 */
void saveBeforeReboot();
```

---

## Summary

This API reference provides detailed documentation for the core classes in the ESP32-P4 AllSky Display firmware. Each class is designed with a specific responsibility, following SOLID principles for maintainability.

**Key Design Patterns:**
- **Singleton Pattern:** All manager classes use global instances (`displayManager`, `mqttManager`, etc.)
- **Separation of Concerns:** Each class handles one aspect (display, network, storage, etc.)
- **RAII Pattern:** `pauseDisplay()`/`resumeDisplay()` should use RAII guards for exception safety
- **Callback Pattern:** Debug functions use function pointers for decoupled logging

**Memory Safety:**
- Always check buffer sizes before operations
- Use DMA-aligned allocators for PPA buffers
- Reset watchdog in long-running operations
- Monitor heap/PSRAM via SystemMonitor

**Error Handling:**
- Most methods return `bool` for success/failure
- Check return values before proceeding
- Use `isAvailable()` checks for optional hardware (PPA, touch, etc.)
- Graceful degradation (e.g., software fallback if PPA fails)

For usage examples and integration patterns, see `ARCHITECTURE.md` and the main firmware code.
