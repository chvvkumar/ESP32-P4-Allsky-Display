# External Integrations

**Analysis Date:** 2026-06-28

## Image Sources

**AllSky Camera (primary user-configured source):**
- Protocol: HTTP or HTTPS JPEG fetch via `HTTPClient`
- URL: User-configured at runtime, stored in NVS via `config_storage.cpp` (`getImageURL()` / `getImageSource(i)`)
- Default placeholder in firmware: `http://allskypi5.lan/current/resized/image.jpg` (`config_storage.cpp` line 52)
- Supports up to 10 concurrent image sources (`MAX_IMAGE_SOURCES = 10`, `config.h`)
- Update interval: configurable, default 120 seconds (`UPDATE_INTERVAL`, `config.h`)
- Cache headers respected; forced re-fetch after 15 minutes regardless (`FORCE_CHECK_INTERVAL`, `config.h`)

**Moon Computed Source:**
- Sentinel URL scheme: `moon://default` (stored as an image source URL, detected by `startsWith("moon://")`)
- No network fetch; image is rendered locally by `moon_sphere_render()` in `moon_sphere.cpp`
- Texture data: equirectangular map in `moon_equirect_data.h`, decoded at first use by `stb_image`
- Ephemeris computed locally in `moon_ephemeris.c` / `moon_interaction.c`
- Requires accurate system clock (see Time Synchronization below)

## Image Presets (built-in HTTPS sources)

All presets defined in `image_presets.cpp`. Fetched via HTTPS `HTTPClient`. No API key required (public endpoints).

| ID | Label | URL | Nominal Size |
|----|-------|-----|-------------|
| `sdo_aia_304` | Sun SDO/AIA 304A | `https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0304.jpg` | 1024px |
| `sdo_aia_171` | Sun SDO/AIA 171A | `https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0171.jpg` | 1024px |
| `sdo_aia_193` | Sun SDO/AIA 193A | `https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0193.jpg` | 1024px |
| `sdo_hmi_igr` | Sun SDO/HMI Cont. | `https://soho.nascom.nasa.gov/data/realtime/hmi_igr/1024/latest.jpg` | 1024px |
| `sdo_hmi_mag` | Sun SDO/HMI Magn. | `https://soho.nascom.nasa.gov/data/realtime/hmi_mag/1024/latest.jpg` | 1024px |
| `soho_c2` | Sun SOHO LASCO C2 | `https://soho.nascom.nasa.gov/data/realtime/c2/1024/latest.jpg` | 1024px |
| `soho_c3` | Sun SOHO LASCO C3 | `https://soho.nascom.nasa.gov/data/realtime/c3/1024/latest.jpg` | 1024px |
| `goes19_full` | Earth GOES-19 | `https://cdn.star.nesdis.noaa.gov/GOES19/ABI/FD/GEOCOLOR/1808x1808.jpg` | 1808px |

Source agencies: NASA SDO, NASA/ESA SOHO, NOAA GOES-19. All are public HTTPS image endpoints, no authentication.

The 1808px GOES-19 image exceeds the display-derived download buffer; `MIN_DOWNLOAD_BUFFER_SIZE` is set to 3MB in `config.h` to handle it.

## MQTT Broker

**Purpose:** Bidirectional device control and telemetry, Home Assistant MQTT integration.

- Library: `PubSubClient` 2.8
- Implementation: `mqtt_manager.cpp`, `mqtt_manager.h`
- Connection: TCP, configurable broker host/port/user/password stored in NVS
- Config keys: `MQTT_SERVER`, `MQTT_PORT`, `MQTT_USER`, `MQTT_PASSWORD`, `MQTT_CLIENT_ID` (`config.h` / NVS)
- Default port: 1883 (standard MQTT, user-configurable via web UI)
- Reconnect interval: configurable, default 5 seconds (`MQTT_RECONNECT_INTERVAL`, `config.h`)
- Reconnect uses exponential backoff (`mqtt_manager.cpp`)

**Subscribed topics (inbound commands):**
- Base topic derived from device MAC address and HA discovery prefix
- Command topic filter: built by `HADiscovery::getCommandTopicFilter()` in `ha_discovery.cpp`
- Handles: brightness, cycling on/off, next image, force refresh, image source selection, and other entity commands

**Published topics (outbound state):**
- Availability/heartbeat topic: published periodically by `publishAvailabilityHeartbeat()`
- State topic: full device state JSON published on change
- Attributes topic: extended attributes JSON
- Sensor topics: current image URL, memory stats, WiFi RSSI, uptime (28 entities total, `HA_DISCOVERY_TOTAL_STEPS = 28` in `ha_discovery.h`)

## Home Assistant MQTT Discovery

**Purpose:** Auto-registration of device entities in Home Assistant.

- Implementation: `ha_discovery.cpp`, `ha_discovery.h`
- Triggered on MQTT connect; non-blocking state machine publishes one entity per call
- Discovery prefix: configurable (default `homeassistant`), stored in NVS
- Entity types published: `light` (brightness), `switch` (cycling, etc.), `number` (intervals, scale), `select` (image source), `button` (next image, restart), `sensor` (image URL, memory, RSSI, uptime)
- Device ID: derived from MAC address for uniqueness

## Home Assistant REST API (Brightness Auto-Control)

**Purpose:** Poll a HA light sensor entity via REST API and auto-adjust display brightness.

- Implementation: `ha_rest_client.cpp`, `ha_rest_client.h`
- Uses `HTTPClient` with Bearer token auth (long-lived HA access token)
- Runs in a dedicated FreeRTOS task pinned to Core 0 (`HA_REST_TASK_CORE = 0`)
- Config stored in NVS (set via web UI Advanced page):
  - `haBaseUrl`: Home Assistant base URL (e.g., `http://homeassistant.local:8123`)
  - `haAccessToken`: Long-lived access token (Bearer)
  - `haLightSensorEntity`: Entity ID of the light sensor (e.g., `sensor.living_room_lux`)
  - `lightSensorMinLux`, `lightSensorMaxLux`: Lux range for brightness mapping
  - `displayMinBrightness`, `displayMaxBrightness`: Output brightness range (0-100)
  - `haPollInterval`: Poll frequency in ms
  - `lightSensorMappingMode`: 0=Linear, 1=Logarithmic, 2=Threshold
- REST endpoint called: `GET <haBaseUrl>/api/states/<haLightSensorEntity>`
- Feature is disabled by default; enabled via `useHARestControl` NVS flag

## HTTP Web Configuration Server

**Purpose:** Browser-based configuration UI, REST API for control, OTA firmware upload.

- Implementation: `web_config.cpp`, `web_config_api.cpp`, `web_config_pages.cpp`, `web_config_html.h`
- Library: `WebServer` (bundled with Arduino-ESP32)
- Port: **8080** (started at `webConfig.begin(8080)` in `.ino` line 822)
- Routes served:
  - `/` - main status page
  - `/console` - WebSocket log console UI
  - `/network`, `/mqtt`, `/image`, `/display`, `/advanced` - configuration pages
  - `/status` - JSON status endpoint
  - `/api/save` - POST config values (used by HA `rest_command` integration)
  - `/api/next-image`, `/api/add-source`, `/api/remove-source`, `/api/update-source` - image management
  - `/api/force-refresh`, `/api/restart`, `/api/factory-reset` - device control
  - `/api/backup` - GET config backup JSON
  - `/api/restore` - POST config restore JSON
  - `/api/screenshot` - GET current display frame
  - `/api/health` - GET device health JSON
  - `/api/wifi-scan` - GET WiFi network scan results
  - `/update` - ElegantOTA firmware upload page

**ElegantOTA:**
- Library: `ElegantOTA` 3.1.7
- Mounted on the web config `WebServer` instance
- Upload endpoint: `http://<device-ip>:8080/update`
- Binary file format: `.bin` produced by `arduino-cli compile --output-dir`

## WebSocket Console

**Purpose:** Real-time log streaming from device to browser developer console.

- Library: `WebSocketsServer` (`WebSockets` 2.7.2, Markus Sattler)
- Port: **81** (ws://device-ip:81)
- Started alongside web config server (`web_config.cpp` line 125)
- Direction: server-to-client push only (device broadcasts log lines)
- Severity filtering: client can set minimum log level; device filters before sending
- Each log message includes timestamp prefix and severity label (added by `broadcastLog()` in `web_config.cpp`)
- Also mirrors to Serial and crash logger (RTC memory) simultaneously

## ArduinoOTA

**Purpose:** Wireless firmware upload from Arduino IDE or `arduino-cli upload`.

- Library: `ArduinoOTA` (bundled with Arduino-ESP32)
- Implementation: `network_manager.cpp` lines 403-483
- Port: **3232** (default Arduino OTA port)
- Hostname: `esp32-allsky-display` (mDNS advertised)
- Authentication: disabled in current code (line 409 commented out)
- Triggers: same OTA progress display on-screen as ElegantOTA

## Captive Portal (First-Boot WiFi Setup)

**Purpose:** Allow WiFi credential entry on first boot or when no credentials are stored.

- Implementation: `captive_portal.cpp`, `captive_portal.h`
- Libraries: `WebServer`, `DNSServer` (both bundled with Arduino-ESP32)
- AP SSID: `AllSky-Display-Setup` (default, defined in `captive_portal.h` line 17)
- AP password: none (open network by default)
- DNS: all DNS queries redirected to device AP IP (standard captive portal pattern)
- Timeout: 5 minutes (`CAPTIVE_PORTAL_TIMEOUT = 300000`, `config.h`)
- After successful WiFi config, redirects browser to `http://<device-ip>:8080`
- Triggers on first boot when `!configStorage.isWiFiProvisioned()`

## Time Synchronization

**Purpose:** Accurate system clock for moon ephemeris calculation and log timestamps.

**HTTP Date Header method (primary, working on ESP32-P4):**
- Implementation: `network_manager.cpp` (`syncTimeViaHttp()`, `fetchHttpDate()`)
- Mechanism: performs HTTP HEAD/GET to a reliable server, parses `Date:` response header, sets `settimeofday()`
- Triggered on WiFi connect, then every 6 hours (`HTTP_TIME_RESYNC_INTERVAL = 21600000`, `network_manager.h`)
- Retried every 20 seconds while clock is invalid (`HTTP_TIME_RETRY_INTERVAL = 20000`)
- Note: SNTP/UDP does not work on this hardware (ESP32-P4 + ESP-Hosted); HTTP Date is the only functional path (comment in `network_manager.h` lines 37-39)

**NTP (fallback, non-functional on this hardware):**
- Methods `syncNTPTime()` and `isTimeValid()` exist in `WiFiManager` but SNTP does not work on ESP32-P4/ESP-Hosted
- NTP server and timezone settings are stored in NVS and exposed in web UI (for future use or hardware revision)

## Data Storage

**NVS (Non-Volatile Storage):**
- Implementation: `config_storage.cpp` using `Preferences` library
- Namespace: single NVS namespace (defined as `ConfigStorage::NAMESPACE`)
- Thread-safe: FreeRTOS mutex (`SemaphoreHandle_t _mutex`) wraps all reads/writes
- Dirty-bit system: 13 field-group bitmasks (`DIRTY_*` in `config_storage.h`) track which NVS groups need writing; only dirty groups are written on `saveConfig()` to reduce NVS wear
- Config schema version: `CONFIG_SCHEMA_VERSION = 1` in `config.h`; migration hook in `config_backup.cpp`

**Crash Logger (RTC memory):**
- Implementation: `crash_logger.cpp`, `crash_logger.h`
- Stores recent log lines in RTC slow memory (survives soft reset)
- Exposed via web UI "Clear Crash Logs" action and `/api/crash-logs` (sent to WebSocket client on connect)

**File Storage:**
- SPIFFS partition (`data`, 12MB) defined in `partitions.csv`
- Not actively used by current firmware for image storage; all image buffers are PSRAM heap

## WiFi

**Mode:** Station (STA) with AP fallback for setup.

- AP roaming (mesh network support): scans for better AP every 60 seconds, switches if RSSI improves by more than 8 dB (`WIFI_ROAM_RSSI_THRESHOLD`, `config.h`)
- WiFi credentials: stored in NVS via `ConfigStorage`
- QR code displayed on-screen for captive portal AP URL (`wifi_qr_code.h`)
- WiFi scan results available via `/api/wifi-scan` REST endpoint

## CI/CD and Release

**Runner:** Self-hosted GitHub Actions runner (`runs-on: self-hosted`)
- Triggered on push to `main` or `snd` branches, or PR against `main`/`snd`/`Dev`
- Workflow: `.github/workflows/arduino-compile.yml`

**Release artifacts:**
- `.bin` firmware binary built with `arduino-cli compile` + `--output-dir ./build`
- Merged binary created with `esptool merge_bin` for single-file OTA
- GitHub Release created automatically on push to `main` (stable) or `snd` (test)
- Memory usage badge JSON committed to `.github/badges/` branch for shields.io display

**Secrets required in GitHub repository:**
- `GITHUB_TOKEN` (standard, used for release creation)
- No external service secrets needed for CI build

---

*Integration audit: 2026-06-28*
