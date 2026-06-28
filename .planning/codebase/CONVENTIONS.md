# Coding Conventions

**Analysis Date:** 2026-06-28

## File Naming

**Pattern:** `snake_case` for all source files.

- Module implementation pairs: `config_storage.cpp` / `config_storage.h`, `web_config_pages.cpp` / `web_config.h`
- Main sketch: `ESP32-P4-Allsky-Display.ino` (kebab-case, Arduino requirement)
- Header-only utilities: `logging.h`, `watchdog_scope.h`, `web_config_html.h`, `wifi_qr_code.h`
- CI enforces: filenames must match `^[a-zA-Z0-9_-]+\.(cpp|h|ino)$` (see `.github/workflows/arduino-compile.yml` line 94)
- CI warns when a `.h` file lacks a matching `.cpp`, with explicit exceptions for `web_config_html` and `displays_config`

## Class vs Free Function Design

**Pattern:** One class per module, exposed as a global singleton instance.

Each module defines a class in its `.h` and instantiates it as a file-scope global in its `.cpp`:

```cpp
// config_storage.cpp
ConfigStorage configStorage;  // global singleton

// web_config.cpp
WebConfig webConfig;          // global singleton
```

The `.ino` sketch and other modules reference these globals via `extern` declarations or by direct include. Free functions are used only for callbacks, FreeRTOS tasks, and JPEG decoder callbacks (e.g., `JPEGDraw`, `downloadTask`).

## Naming Patterns

**Classes:** `PascalCase` — `ConfigStorage`, `WebConfig`, `DisplayManager`, `CrashLogger`, `WiFiManager`, `WatchdogScope`, `ConfigLock`

**Class member functions:** `camelCase` — `loadConfig()`, `saveConfig()`, `getWiFiSSID()`, `setMQTTServer()`, `isConnected()`, `getBrightness()`

**Global free functions:** `camelCase` — `logPrint()`, `logPrintf()`, `generateDocLink()`, `formatUptime()`, `escapeHtml()`

**Global variables (sketch-level):** `camelCase` — `imageBuffer`, `currentImageIndex`, `cyclingEnabled`, `firstImageLoaded`

**Constants and macros:** `UPPER_SNAKE_CASE` — `UPDATE_INTERVAL`, `MAX_IMAGE_SOURCES`, `DEFAULT_SCALE_X`, `WATCHDOG_TIMEOUT_MS`

**Enum values:** `UPPER_SNAKE_CASE` prefixed by type — `LOG_DEBUG`, `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`, `LOG_CRITICAL` (in `config.h`); `TOUCH_IDLE`, `TOUCH_PRESSED` (in `.ino`)

**Dirty-field bitmask constants:** `UPPER_SNAKE_CASE` with `DIRTY_` prefix — `DIRTY_WIFI`, `DIRTY_MQTT`, `DIRTY_HA_DISC` (in `config_storage.h`)

**Private member variables:** underscore-prefixed — `_mutex`, `_dirty`, `_dirtyFields`, `_acquired`, `_paused`

**FreeRTOS handles:** `camelCase` with descriptive suffix — `downloadTaskHandle`, `imageReadyQueue`, `imageBufferMutex`

## Header Guard Style

Both `#pragma once` and traditional include guards are used together on all headers:

```cpp
#pragma once
#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H
// ...
#endif // CONFIG_STORAGE_H
```

`config.h` uses both. Some newer headers (e.g., `watchdog_scope.h`, `network_manager.h`) use both forms. This dual-guard pattern is the established convention.

## Arduino String vs char*

**Pattern:** Arduino `String` class is used exclusively for all runtime string operations. `const char*` is used only for compile-time literals, `PROGMEM` constants, and C-style API boundaries (logging functions, `Serial.println`).

- Config values stored in `ConfigStorage` as `String` fields
- HTML building uses `String` concatenation (see `web_config_pages.cpp`)
- Log macros accept `const char*` literals: `LOG_INFO("[WebAPI] Config save")`
- Printf-style variants accept format strings: `LOG_INFO_F("[WebAPI] SSID: %s\n", value.c_str())`
- Conversion at boundaries: `.c_str()` when passing `String` to `const char*` APIs

## PROGMEM Usage

Large static web assets (CSS, JavaScript, HTML templates, modal markup) are stored in flash using `PROGMEM` with raw string literals. The pattern is exclusively in `web_config_html.h`:

```cpp
const char HTML_CSS[] PROGMEM = R"rawliteral(
/* minified CSS here */
)rawliteral";

const char HTML_JAVASCRIPT[] PROGMEM = R"rawliteral(
/* minified JS here */
)rawliteral";

const char HTML_MODALS[] PROGMEM = R"rawliteral(
<!-- modal HTML here -->
)rawliteral";

const char HTML_IMAGES_APP[] PROGMEM = R"rawliteral(
<!-- images app HTML here -->
)rawliteral";
```

These are served via `FPSTR()` and `server->sendContent(FPSTR(HTML_CSS))` to read directly from flash without copying to heap (`web_config.cpp` lines 200, 210, 213).

## Raw String Literals R"rawliteral(...)"

All embedded HTML/CSS/JavaScript blocks use the `R"rawliteral(...)rawliteral"` delimiter form (not the shorter `R"(...)"`) to avoid conflicts with closing parentheses inside web content. This is the only place raw string literals appear in the codebase. The delimiter `rawliteral` is consistent across all four `PROGMEM` constants in `web_config_html.h`.

## Dynamic HTML Construction

All page HTML is built by string concatenation into a pre-allocated Arduino `String`:

```cpp
String html;
html.reserve(12000);  // Pre-allocate to prevent heap fragmentation
html = "<div class='main'><div class='container'>";
html += "<div class='stat-card'>" + String(someValue) + "</div>";
```

Key patterns:
- `html.reserve(N)` is called immediately after declaration to pre-allocate estimated size
- Integer/float values converted with `String(value)` before concatenation
- User-supplied values always wrapped in `escapeHtml()` before insertion into HTML attributes or content
- Static assets (CSS, JS) served as chunked PROGMEM reads, never concatenated into the page string
- Page rendering uses chunked transfer (`server->setContentLength(CONTENT_LENGTH_UNKNOWN)`) with `beginChunkedHtmlResponse()` / `endChunkedHtmlResponse()` wrappers (`web_config.cpp` lines 191, 207)

## escapeHtml Pattern

Both `WebConfig` and `CaptivePortal` classes implement their own `escapeHtml(const String& input)` private method (`web_config.h` line 126, `captive_portal.h` line 67). All user-controlled values placed inside HTML attributes or visible text must pass through `escapeHtml()`:

```cpp
html += "<input value='" + escapeHtml(configStorage.getWiFiSSID()) + "'>";
html += "<p>" + escapeHtml(configStorage.getMQTTClientID()) + "</p>";
```

## Logging Approach

All logging goes through macros defined in `logging.h`. Direct `Serial.println()` / `Serial.printf()` calls are forbidden except in `ConfigStorage` mutex timeout warnings and early-init paths before the logging system initializes.

**Macro family:**

```cpp
LOG_DEBUG("message")              // stripped at compile time when NDEBUG defined
LOG_DEBUG_F("[Module] fmt %d\n", val)
LOG_INFO("message")
LOG_INFO_F("[Module] fmt %s\n", str)
LOG_WARNING("message")
LOG_WARNING_F(...)
LOG_ERROR("message")
LOG_ERROR_F(...)
LOG_CRITICAL("message")
LOG_CRITICAL_F(...)
```

**Convention:** Log messages include a `[ModuleName]` prefix in square brackets: `"[WebServer] Initializing"`, `"[WebAPI] Configuration save"`, `"[WebServer] Dashboard page accessed"`.

The underlying `logPrint()` and `logPrintf()` (implemented in `.ino`) route messages to both `Serial` and the WebSocket console, filtered by the runtime-configurable `LogSeverity` level stored in `configStorage`.

## Config Storage Pattern

All persistent settings live in a single `ConfigStorage` class (`config_storage.h` / `config_storage.cpp`) backed by ESP32 NVS via the `Preferences` library under namespace `"allsky_config"`.

- Fields are grouped into dirty bitmasks (`DIRTY_WIFI`, `DIRTY_MQTT`, `DIRTY_DISPLAY`, etc.)
- `markDirty(uint32_t fields)` is called by every setter
- `saveConfig()` only writes groups that have dirty bits set, reducing flash wear
- Thread safety via `ConfigLock` RAII guard (wraps `xSemaphoreTake` / `xSemaphoreGive` with 1-second timeout)
- The global singleton is `configStorage` (defined in `config_storage.cpp`, used across all modules)

Schema versioning: `CONFIG_SCHEMA_VERSION` macro in `config.h` controls backup/restore migration (`config_backup.cpp`).

## Constants vs Magic Numbers

All timing, threshold, buffer size, and pin constants are defined as `#define` in `config.h`. No magic numbers appear in module code. Examples: `UPDATE_INTERVAL`, `WATCHDOG_TIMEOUT_MS`, `BACKLIGHT_PIN`, `MIN_DOWNLOAD_BUFFER_SIZE`, `CRITICAL_HEAP_THRESHOLD`.

## RAII Patterns

Two RAII guard classes are in active use:

- `ConfigLock` (`config_storage.h` lines 28-51): mutex guard for `ConfigStorage` operations
- `WatchdogScope` / `WATCHDOG_SCOPE()` macro (`watchdog_scope.h`): resets hardware watchdog on scope entry and exit

Both classes delete copy constructor and assignment operator. `WatchdogScope` allows move.

## Error Handling

- Functions that can fail return `bool` (e.g., `ConfigStorage::begin()`, `DisplayManager::begin()`)
- Allocation failures are checked explicitly: `if (!server) { LOG_CRITICAL(...); return false; }`
- `try/catch` is used around WebServer initialization in `web_config.cpp` (lines 23-...)
- Hardware watchdog (`WATCHDOG_TIMEOUT_MS = 90000ms`) acts as the last-resort recovery for hangs
- Crash logger (`crash_logger.h`) preserves a ring buffer in RTC memory across soft reboots for post-mortem diagnostics

## Comment Style

- Section separators use `// ===...===` with a descriptive label (seen throughout `config.h`)
- Block comments on classes and RAII types use `/** @brief ... */` JSDoc style (`watchdog_scope.h`)
- Inline comments explain non-obvious decisions: buffer sizing math, hardware workarounds, NTP quirks
- No `TODO`/`FIXME` markers present in any `.cpp` or `.h` file (confirmed by search)

## Include Order

Headers are included in this order within `.cpp` files:
1. Own module header (e.g., `#include "web_config.h"`)
2. Related project headers (`#include "web_config_html.h"`, `#include "system_monitor.h"`)
3. Arduino/ESP32 SDK headers (`#include <Arduino.h>`, `#include <time.h>`)
4. Third-party library headers (`#include <JPEGDEC.h>`, `#include <PubSubClient.h>`)

---

*Convention analysis: 2026-06-28*
