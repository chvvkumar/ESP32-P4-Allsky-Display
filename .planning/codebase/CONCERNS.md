# Codebase Concerns

**Analysis Date:** 2026-06-28

---

## 1. Web UI Maintainability: Hardcoded Colors and Inline Styles

**Files:** `web_config_pages.cpp` (1,464 lines), `web_config_html.h` (484 lines)

**Issue:** The page-generation layer contains ~502 occurrences of `style='...'` inline attributes and ~414 hex color literals scattered across `web_config_pages.cpp` alone. An additional ~79 hex literals appear in `web_config_html.h`. Representative examples at `web_config_pages.cpp:54,56,57,170,171,172` show patterns like:

```cpp
html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:0.5rem;margin-top:0.75rem;font-size:0.9rem;color:#94a3b8'>";
html += "<div><strong style='color:#64748b'>IP Address:</strong><br>" + WiFi.localIP().toString() + "</div>";
```

Every color value (`#94a3b8`, `#64748b`, `#38bdf8`, `#10b981`, `#ef4444`, `#334155`, `#1e293b`, `#0f172a`) is repeated verbatim dozens of times in runtime-built C++ strings. Changing the theme palette requires grep-and-replace across both files simultaneously, with no compile-time safety check.

**The :root partial fix:** A single `:root` block exists at `web_config_html.h:118` declaring CSS custom properties (`--accent`, `--surface`, `--ok`, etc.). However, these variables are consumed only by the image-editor component classes (`.img-*`, `web_config_html.h:119-166`). All other pages ignore the design tokens entirely and repeat raw hex values.

**Impact for IDF migration:** Any port to ESP-IDF (`esp_http_server`) must either replicate the same sprawl or refactor the color system first. If moving to a separate HTML file or template system, the lack of CSS variables means a manual find-and-replace touching hundreds of strings.

**Fix approach:** Extend the `:root` block's existing design tokens to cover all named colors, then mechanically replace inline hex literals with `var(--name)` in the CSS and move per-component style declarations into named CSS classes in `HTML_CSS` in `web_config_html.h`.

---

## 2. Web UI Maintainability: String Concatenation Architecture

**Files:** `web_config_pages.cpp`, `web_config.cpp`

**Issue:** All nine page generators build HTML via repeated `html += "..."` string concatenation (502+ concatenation statements in `web_config_pages.cpp`). Pages pre-allocate with `html.reserve(N)`:

- `generateMainPage()` line 23: `html.reserve(12000)` (~12 KB)
- `generateAPIReferencePage()` line 778: `html.reserve(15000)` (~15 KB)
- `generateConsolePage()` line 1222: `html.reserve(10000)` (~10 KB)
- `generateSerialCommandsPage()` line 639: `html.reserve(8000)` (~8 KB)
- `generateNetworkPage()` line 184: `html.reserve(8000)` (~8 KB)

While `reserve()` avoids most reallocations, the entire page content must exist simultaneously in heap as a `String` object before `sendContent()` is called. For the API reference page reserving 15 KB, the actual generated content likely exceeds this, causing at least one internal realloc. The page `String` plus the `generateHeaderBody` / `generateNavigation` / `generateFooterBody` strings co-exist in heap during the response send window.

**Impact for IDF migration:** `esp_http_server` handlers use a chunked-send model with `httpd_resp_send_chunk(req, buf, len)`. The current architecture can map onto this, but the generator functions return `String` objects, meaning the full page content must still be heap-resident before chunking begins. The refactor opportunity is to stream directly from generators rather than building a complete string.

**Fix approach:** Convert page generators to accept a callback or output buffer parameter and write small fragments directly to `httpd_resp_send_chunk()` rather than accumulating a `String`.

---

## 3. Escaped-String Fragility: Injected JavaScript in C++ Strings

**Files:** `web_config_pages.cpp`, `web_config.cpp`

**Issue:** Page-specific JavaScript is injected via `html += "..."` statements containing escaped quotes. The network page (`web_config_pages.cpp:215-261`) builds a full WiFi scan JS block this way, with patterns such as:

```cpp
html += "html+='<div style=\"display:flex;align-items:center;justify-content:between;padding:0.75rem;...\" onclick=\"selectNetwork(\\'" + ...
html += "html+='<strong style=\"color:#e2e8f0\">'+n.ssid+'</strong></div>';";
```

The backup/restore JS (`web_config_pages.cpp:600-624`) uses the alternative `"..."` string concatenation without a raw literal, producing C-string escapes three levels deep (C++ string escape, HTML attribute string, JavaScript string). The screenshot JS (`web_config_pages.cpp:570-583`) concatenates a single C++ string literal split across multiple continuation lines with no escaping layer, which is safer but inconsistent.

Separate JavaScript in `HTML_JAVASCRIPT` (`web_config_html.h:168-209`) correctly uses `R"rawliteral(...)rawliteral"` and is not affected. Only page-injected, per-page scripts carry the fragility.

**Impact for IDF migration:** ESP-IDF `httpd` has no built-in template engine. Moving to a separate static `.html` file (served from SPIFFS/LittleFS or embedded via `xxd`) would eliminate the escaping problem entirely by separating JS from C++ strings.

**Fix approach:** Move all page-specific JS into the `R"rawliteral"` block in `web_config_html.h` (or a dedicated `web_config_images_js.h` etc.), parameterized by injected data through a JSON API call at page load rather than server-side string interpolation.

---

## 4. Arduino WebServer vs ESP-IDF esp_http_server Differences

**Files:** `web_config.cpp`, `web_config.h`, `captive_portal.cpp`

**Issue:** The web layer is built on Arduino `WebServer` (from the `esp32` Arduino core) and `WebSocketsServer` (ArduinoWebSockets library). The chunked response design uses Arduino-specific APIs:

```cpp
// web_config.cpp:191-204
server->setContentLength(CONTENT_LENGTH_UNKNOWN);
server->send(200, "text/html", "");
server->sendContent(FPSTR(HTML_CSS));       // Flash string
server->sendContent(generateHeaderBody(title));
server->sendContent(generateNavigation(navPage));
```

The `FPSTR()` / `F()` macro idiom and `CONTENT_LENGTH_UNKNOWN` are Arduino abstractions. ElegantOTA (`web_config.cpp:84-115`) also depends on Arduino `WebServer`. The WebSocket server is a second listener on port 81 (`web_config.cpp:125`), allocated as a separate `WebSocketsServer` instance.

Key behavioral differences between Arduino `WebServer` and `esp_http_server`:
- Arduino `WebServer` uses a single-threaded poll model (`server->handleClient()` in `loop()`). `esp_http_server` is task-based and re-entrant by default.
- Arduino route registration uses lambda captures of `this` (`web_config.cpp:35-76`); IDF uses `httpd_uri_t` structs with a `user_ctx` void pointer.
- `CONTENT_LENGTH_UNKNOWN` / chunked transfer encoding must be set explicitly in IDF via `httpd_resp_send_chunk()`.
- `FPSTR()` is meaningless in IDF; PROGMEM is an Arduino/AVR concept. On ESP32 all flash strings are already directly accessible, so `FPSTR` is a no-op wrapper, but code depending on it cannot be mechanically ported to non-Arduino toolchains.
- WebSocket support is not built into `esp_http_server`; would require `esp_websocket_client` or a separate library.
- ElegantOTA has an IDF-compatible variant but requires API changes.

**Impact:** A direct port is not line-for-line possible. The chunked response helpers `beginChunkedHtmlResponse` / `endChunkedHtmlResponse` in `web_config.cpp:191-217` will need full rewrites. The 35+ route registrations in `web_config.cpp:35-77` must each become an `httpd_uri_t` with corresponding handler function (not lambda).

---

## 5. Web-to-Firmware Coupling: Global Singleton Dependencies

**Files:** `web_config_pages.cpp`, `web_config_api.cpp`, `web_config.h`

**Issue:** The web layer directly calls methods on six firmware globals without any abstraction layer:

- `configStorage` (declared `extern ConfigStorage configStorage` in `config_storage.h:391`)
- `wifiManager` (declared `extern WiFiManager wifiManager` in `network_manager.h:112`)
- `mqttManager` (declared `extern MQTTManager mqttManager` in `mqtt_manager.h:71`)
- `displayManager` (declared `extern DisplayManager displayManager` in `display_manager.h:84`)
- `systemMonitor` (used via `webConfig.cpp`)
- `crashLogger` (declared locally `extern CrashLogger crashLogger` in `web_config_api.cpp:20`)

In `web_config_pages.cpp` alone there are ~69 direct calls to these globals (grep count). The page generators call `configStorage.getMQTTServer()`, `wifiManager.isConnected()`, `WiFi.SSID()`, `displayManager.getBrightness()` etc. inline during HTML generation.

Additionally, `webConfig` is itself referenced from `logging.h:10` (`extern WebConfig webConfig`), making the logging system dependent on the web layer, which creates a near-circular dependency chain: every module that logs triggers a `webConfig.broadcastLog()` call.

**Impact for IDF migration:** A move to ESP-IDF cannot change these dependencies without also restructuring module boundaries. The web handlers are not testable in isolation. If the goal is to run IDF http handlers on a separate FreeRTOS task (the IDF default), these global accesses become potential concurrency hazards unless each global is mutex-protected or the handlers are pinned to the same core as the modules they call.

**Existing mutex protection:** `configStorage` has a mutex (`config_storage.cpp:13`). `displayManager`, `wifiManager`, `mqttManager` do not have equivalent thread-safety documentation.

**Fix approach:** Introduce a lightweight `WebContext` struct passed to all handler functions, holding pointers or snapshots of the values needed, rather than each handler reaching directly into globals.

---

## 6. No Web Authentication

**Files:** `web_config.cpp`, `captive_portal.cpp`

**Issue:** All 35+ HTTP endpoints are unauthenticated. The destructive endpoints `/api/factory-reset` (`web_config_api.cpp:1088`) and `/api/restart` (`web_config_api.cpp:339`) require only a POST with no token, CSRF check, or session cookie. Any device on the same LAN can reset or reboot the device by sending a single HTTP POST. The backup endpoint `/api/backup?secrets=1` (`web_config_api.cpp:1109`) can exfiltrate WiFi password, MQTT password, and HA long-lived access token in plaintext JSON.

The web UI itself warns about the backup risk (`web_config_pages.cpp:590`) but the server enforces nothing.

**Impact:** This is acceptable for a private LAN device but becomes critical if the device is ever bridged to a less-trusted network or if a captive-portal bypass occurs. For an IDF migration this is the right time to add digest auth or a simple shared secret header check.

---

## 7. CDN Dependencies That Fail on Isolated LAN

**Files:** `web_config_html.h:11-12`, `captive_portal.cpp:294`

**Issue:** The CSS block loaded on every page via `FPSTR(HTML_CSS)` begins with two external `@import` statements:

```css
@import url('https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css');
@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@300;400;500;700&display=swap');
```

The captive portal also imports the Google font (`captive_portal.cpp:294`). On an isolated LAN with no internet access (the primary deployment scenario for many AllSky setups), these imports silently fail. The browser falls back to system fonts, Font Awesome icons render as Unicode replacement characters or empty boxes, and the navigation / status badges lose their icon glyphs entirely.

Font Awesome is used pervasively across all pages:
- `<i class='fas fa-clock stat-icon'>`, `<i class='fas fa-satellite'>`, `<i class='fas fa-sync-alt'>`, `<i class='fas fa-forward'>`, `<i class='fas fa-question-circle'>`, `<i class='fas fa-info-circle'>`, and many more.
- The GitHub link at `web_config.cpp:329` uses `<i class='github-icon fa-github'>` which depends on `Font Awesome 6 Brands` font-family declared at `web_config_html.h:49`.

**Impact:** On an air-gapped or LAN-only install the UI is functional but visually broken. An IDF port that also modernizes the UI should bundle Font Awesome as a base64-embedded subset or switch to Unicode symbols or SVG icons.

**Fix approach:** Either embed a minimal Font Awesome subset as base64 inside `HTML_CSS`, use CDN with a local fallback `<link>` tag, or replace all `<i class='fas ...'>` usage with Unicode symbols or inline SVG.

---

## 8. Heap Pressure: Per-Request String Building

**Files:** `web_config.cpp`, `web_config_pages.cpp`

**Issue:** Each web request allocates one or more large `String` objects on the internal heap (not PSRAM). Page generators reserve 4-15 KB (`html.reserve(4000)` through `html.reserve(15000)`). These live on the Arduino/ESP-IDF heap alongside the firmware's other heap consumers (WebSocket server, MQTT client, HTTPClient for image downloads, crash logger RAM buffer).

The `generateHeader()` fallback path at `web_config.cpp:316-339` allocates the full CSS string via `String(FPSTR(HTML_CSS))` on every 404 hit, copying ~6 KB of PROGMEM CSS into a temporary heap `String`. The chunked path avoids this for normal pages, but the 404 handler does not use the chunked helpers.

Heap logging at `web_config.cpp:221-235` shows heap-before / heap-after comparisons and logs a WARNING when a dashboard request consumes heap that is not returned. No specific threshold triggers a recovery action; the monitoring is informational only.

**Impact:** On a live device actively downloading images (imageBuffer ~3 MB in PSRAM), a browser refresh hitting the dashboard page will allocate ~12 KB of internal heap during response generation. The concurrent presence of a WebSocket broadcast (384-byte stack buffer, `web_config.cpp:602`) is safe. The risk is if multiple simultaneous browser connections each trigger page generation; Arduino `WebServer` processes one client at a time, which limits but does not eliminate the peak.

---

## 9. Watchdog Coupling: Frequent Manual Resets Required

**Files:** `ESP32-P4-Allsky-Display.ino`, `system_monitor.cpp`

**Issue:** The watchdog timeout is set to 90 seconds (`config.h:173`). Because the main loop drives image downloads synchronously in the download task while also calling `server->handleClient()` and `wsServer->loop()` in the main Arduino loop, the code contains a high density of manual `systemMonitor.forceResetWatchdog()` calls throughout image processing and QR code display paths.

In `displayWiFiQRCode()` alone (`ESP32-P4-Allsky-Display.ino:288-513`) there are ~14 explicit `forceResetWatchdog()` calls. The OTA callbacks (`web_config.cpp:91,104`) also call `forceResetWatchdog()` on every progress event.

`WATCHDOG_TRIGGER_PANIC` is `false` (`config.h:175`), meaning a watchdog timeout causes a reset (not a core dump / backtrace). This makes deadlocks harder to diagnose since no panic log is generated.

**Impact for IDF migration:** ESP-IDF task watchdog (`esp_task_wdt`) monitors tasks, not a single thread. If web handling moves to a dedicated IDF task, that task must be added to the watchdog separately with `esp_task_wdt_add()`. The current approach of sprinkling `forceResetWatchdog()` throughout image processing code will not protect a separate web task.

---

## 10. Image Source Hard Cap at 10

**Files:** `config.h:93`, `config_storage.h`, `config_storage.cpp`

**Issue:** `MAX_IMAGE_SOURCES 10` (`config.h:93`) is a compile-time constant. The `configStorage` struct allocates fixed arrays: `config.imageSources[10]`, `config.imageEnabled[10]`, `config.imageDurations[10]`, `config.imageTransforms[10]` (see `config_storage.cpp:62-73`). There is no runtime enforcement that emits a meaningful error; adding an eleventh source silently fails or overwrites the array boundary.

The web API (`web_config_api.cpp:350-367`) calls `configStorage.addImageSource(url)` without checking whether the current count equals `MAX_IMAGE_SOURCES` before the add.

**Fix approach:** Guard `addImageSource()` with a count check and return an error JSON if at capacity. Alternatively increase `MAX_IMAGE_SOURCES` to a larger value and document it as the hard limit.

---

## 11. Global State: Many Module-Level Singletons

**Files:** `ESP32-P4-Allsky-Display.ino` (global scope), `web_config.cpp:13`, `config_storage.cpp:5`, `system_monitor.cpp:5`, `crash_logger.cpp:16`, `network_manager.cpp`, `mqtt_manager.cpp`, `display_manager.cpp`

**Issue:** Every major module exposes a single global instance (`webConfig`, `configStorage`, `systemMonitor`, `crashLogger`, `wifiManager`, `mqttManager`, `displayManager`, `ppaAccelerator`, `haRestClient`). The main `.ino` also declares ~20 global variables for image processing state (`imageBuffer`, `fullImageBuffer`, `pendingFullImageBuffer`, `scaledBuffer`, `scratchBuffer`, `scaledBufferValid`, `imageReadyToDisplay`, `imageGeneration`, etc.).

There is no initialization order guarantee between translation units. The current `setup()` function manually orchestrates initialization order, which works but is fragile: adding a new global that depends on another module requires careful placement in `setup()`.

**Impact for IDF migration:** IDF apps use `app_main()` with explicit task creation. The existing singletons can be retained but the initialization sequence becomes the developer's responsibility with no Arduino `setup()` wrapper. This is achievable but requires documenting the dependency graph.

---

## 12. Render-Critical Global Variables Shared Between Tasks

**Files:** `ESP32-P4-Allsky-Display.ino`

**Issue:** The async download task (`downloadTask`, pinned to Core 0) and the render path in `renderFullImage()` (called from the main loop on Core 1) share several global variables:

- `pendingFullImageBuffer` / `pendingImageWidth` / `pendingImageHeight` (written by download task, read by render)
- `imageReadyToDisplay` (std::atomic<bool>, provides a correct memory barrier)
- `imageBufferMutex` (SemaphoreHandle_t) protects `imageBuffer` during download

`imageReadyToDisplay` is `std::atomic<bool>` which is correct. However, `pendingImageWidth` and `pendingImageHeight` are plain `int16_t` globals (`ESP32-P4-Allsky-Display.ino:82-83`), not atomically updated. The download task writes both before setting `imageReadyToDisplay = true`; the main task reads them after seeing `imageReadyToDisplay == true`. On Xtensa/RISC-V with TSO memory model, the acquire/release semantics of `std::atomic` should enforce ordering, but this is not explicitly documented in the code and relies on the compiler not reordering the writes relative to the atomic store.

`cyclingPausedForEditing`, `currentImageIndex`, `imageSourceCount` are plain globals modified from the web handler path (Core 1, Arduino main task) and read in the download task (Core 0) without any synchronization primitive.

---

## 13. OTA: No Verification of Sketch Integrity Beyond MD5

**Files:** `web_config.cpp:84-115`, `web_config_pages.cpp:556-560`

**Issue:** OTA is handled by ElegantOTA (`web_config.cpp:84`), which validates the incoming binary with MD5 during upload. There is no signature verification, version downgrade protection, or rollback slot. A successful OTA that produces a broken firmware (e.g., fails to connect WiFi) will brick the device until physical serial access is used to reflash.

`WATCHDOG_TRIGGER_PANIC false` (`config.h:175`) means a post-OTA boot loop from a bad firmware does not generate a panic log, making remote diagnosis impossible.

---

## 14. Captive Portal Uses Inline CSS with Same CDN Dependency

**File:** `captive_portal.cpp:294`

**Issue:** The captive portal WiFi setup page inlines `@import url('https://fonts.googleapis.com/css2?family=Roboto...')`. During first-boot WiFi setup, the device operates as an AP with no internet access, so this import always fails silently. The portal is functional but uses the system fallback font. This is a cosmetic issue for first-boot but confusing for users who see a different font than the main UI.

---

## 15. Missing Test Coverage

**Issue:** No test framework is present. There are no `*.test.*` or `*.spec.*` files in the project. All validation is done at runtime on physical hardware. Key untested areas include:

- `configStorage` serialization/deserialization round-trip (NVS Preferences)
- `ConfigBackup::exportJson()` / `importJson()` schema migration path (`config_backup.cpp`)
- Web API parameter parsing in `handleSaveConfig()` (`web_config_api.cpp:27`)
- `escapeHtml()` and `escapeJson()` edge cases (`web_config.cpp:485-520`)
- Image source index bounds checking

The config backup/restore feature introduced `CONFIG_SCHEMA_VERSION 1` (`config.h:29`) with a migration hook in `config_backup.cpp`, but there is no automated test that a v1 backup can be correctly migrated when the version is bumped.

---

*Concerns audit: 2026-06-28*
