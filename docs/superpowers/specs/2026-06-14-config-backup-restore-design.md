# Config Backup and Restore Design

Date: 2026-06-14
Status: Approved for implementation

## Purpose

Let a user export the full device configuration to a downloadable file, then
restore that file onto a device that has been wiped or reset. After a successful
restore the device reboots so the restored settings take effect. The file format
carries a schema version so future firmware can read older backups without data
loss.

## Decisions

- Backup scope: everything, with secrets optional. A checkbox at backup time
  controls whether passwords and tokens are written to the file.
- Version policy: lenient best-effort. Restore applies every field the running
  firmware recognizes, ignores unknown fields, leaves absent fields at their
  current values, and proceeds even when the file's schema version differs.
- Transport: JSON file download and upload through the web UI.

## Context

- Configuration lives in NVS through the `Preferences` API, managed by
  `ConfigStorage` in config_storage.cpp. Roughly 80 scalar fields plus three
  parallel arrays for up to 10 image sources (URL, enabled flag, duration) and a
  per-image transform struct (scaleX, scaleY, offsetX, offsetY, rotation).
- Setters mark per-group dirty bits and many clamp or normalize their input
  (for example `setCycleInterval`, `setImageScaleX`, `setImageRotation`).
  `saveConfig()` writes only dirty groups.
- ArduinoJson 7.4.3 is already installed by the build workflow
  (.github/workflows/arduino-compile.yml). No new dependency or CI change is
  required.
- The web layer hand-builds JSON response strings and registers routes in
  web_config.cpp. `handleFactoryReset` and `handleRestart` in web_config_api.cpp
  show the reboot pattern: respond, `delay(500)`, `crashLogger.saveBeforeReboot()`,
  `ESP.restart()`.
- build_info.h provides `GIT_COMMIT_HASH` for firmware identification.
- There is currently no firmware or config schema version constant.

## Architecture

A new module, `config_backup`, owns the mapping between the `ConfigStorage`
state and the JSON backup document. It is the single place that knows the field
list and the file format. The web layer calls into it and handles transport and
reboot. The UI lives on the existing System settings page.

```
Browser (System page)
  |  GET /api/backup?secrets=0|1            POST /api/restore  (body = file text)
  v                                          v
WebConfig::handleBackup                     WebConfig::handleRestore
  |  ConfigBackup::exportJson(secrets)        |  ConfigBackup::importJson(body)
  v                                           v   (apply via ConfigStorage setters)
ConfigStorage getters                       ConfigStorage setters + saveConfig()
                                             then reboot
```

### Components

1. config.h
   - Add `#define CONFIG_SCHEMA_VERSION 1`. This integer is bumped whenever the
     field set or field semantics change.

2. config_backup.h / config_backup.cpp (new)
   - `String ConfigBackup::exportJson(bool includeSecrets)`
     Builds a `JsonDocument` from `configStorage` getters and returns the
     serialized text. Includes a `_meta` object and a `config` object.
   - `struct ConfigBackup::RestoreResult { bool ok; String error; int fileVersion;
     int applied; int skipped; bool versionMismatch; bool secretsIncluded; };`
   - `RestoreResult ConfigBackup::importJson(const String& body)`
     Parses the document, runs any version migration, applies recognized fields
     through `ConfigStorage` setters, then calls `configStorage.saveConfig()`.
     Does not reboot; the caller does.
   - `static void migrate(JsonObject config, int fromVersion)`
     No-op for version 1. Documented extension point for future renames or
     semantic changes. Additive field changes need no migration because lenient
     apply handles them automatically.

3. web_config.h / web_config.cpp
   - Declare `handleBackup()` and `handleRestore()`.
   - Register `GET /api/backup` and `POST /api/restore` next to the existing
     `/api/factory-reset` and `/api/restart` routes.

4. web_config_api.cpp
   - `handleBackup()`: read the `secrets` query argument, call
     `ConfigBackup::exportJson`, send the result with
     `Content-Disposition: attachment; filename="allsky-config-<deviceName>.json"`
     and content type `application/json`.
   - `handleRestore()`: read the uploaded file text from `server->arg("plain")`,
     call `ConfigBackup::importJson`, send a JSON result describing what was
     applied, then if `ok` reboot using the existing pattern.

5. web_config_pages.cpp
   - Add a "Backup and Restore" card to the System settings page with:
     a "Download backup" button, an "Include passwords and tokens" checkbox,
     a file picker, and a "Restore and reboot" button with a confirmation prompt.
     Restore JS uses `FileReader` to read the chosen file as text and POSTs the
     text as the request body to `/api/restore`.

6. Docs
   - Update docs/03_configuration.md and docs/developer/api_reference.md with the
     feature description and the two endpoints.

## Backup file format

```json
{
  "_meta": {
    "schemaVersion": 1,
    "firmware": "abc1234",
    "deviceName": "ESP32 AllSky Display",
    "displayType": 1,
    "secretsIncluded": true
  },
  "config": {
    "deviceName": "...",
    "wifiProvisioned": true,
    "wifiSSID": "...",
    "wifiPassword": "...",
    "mqttServer": "...", "mqttPort": 1883, "mqttUser": "...", "mqttPassword": "...",
    "mqttClientID": "...",
    "haDiscoveryEnabled": true, "haDeviceName": "...", "haDiscoveryPrefix": "...",
    "haStateTopic": "...", "haSensorUpdateInterval": 30,
    "imageURL": "...",
    "cyclingEnabled": true, "imageUpdateMode": 0, "cycleInterval": 30000,
    "randomOrder": false, "currentImageIndex": 0,
    "imageSources": [
      { "url": "...", "enabled": true, "duration": 30,
        "scaleX": 1.2, "scaleY": 1.2, "offsetX": 0, "offsetY": 0, "rotation": 0 }
    ],
    "defaultBrightness": 50, "brightnessAutoMode": true,
    "defaultScaleX": 1.2, "defaultScaleY": 1.2, "defaultOffsetX": 0,
    "defaultOffsetY": 0, "defaultRotation": 0, "defaultImageDuration": 30,
    "backlightFreq": 5000, "backlightResolution": 10,
    "displayType": 1, "colorTemp": 6500,
    "moonLat": 0, "moonLon": 0, "moonBgStyle": 3,
    "moonFlipU": 0, "moonFlipV": 0, "moonRollOffset": 0, "moonYawOffset": 0,
    "moonPitchOffset": 0, "moonNorthUp": 1, "moonDragLightMode": 0,
    "moonSpinMode": 0, "moonSpinReturnS": 3,
    "updateInterval": 120000, "mqttReconnectInterval": 5000,
    "watchdogTimeout": 90000, "criticalHeapThreshold": 50000,
    "criticalPSRAMThreshold": 100000,
    "minLogSeverity": 0,
    "ntpServer": "pool.ntp.org", "timezone": "CST6CDT,M3.2.0,M11.1.0",
    "ntpEnabled": true,
    "haBaseUrl": "...", "haAccessToken": "...", "haLightSensorEntity": "...",
    "lightSensorMinLux": 0, "lightSensorMaxLux": 300,
    "displayMinBrightness": 10, "displayMaxBrightness": 100,
    "useHaRestControl": false, "haPollInterval": 60, "lightSensorMappingMode": 1
  }
}
```

Secret fields are `wifiPassword`, `mqttPassword`, and `haAccessToken`. When the
backup excludes secrets, these keys are omitted from the file. `wifiSSID`,
`mqttUser`, and other identifiers are not treated as secrets.

## Data flow

### Backup
1. User selects "Download backup", optionally checks "Include passwords and tokens".
2. Browser requests `GET /api/backup?secrets=1` (or `secrets=0`).
3. `handleBackup` calls `ConfigBackup::exportJson(includeSecrets)`.
4. `exportJson` reads every field through `configStorage` getters, builds the
   `_meta` and `config` objects, and serializes.
5. Response is sent as a file attachment; the browser saves it.

### Restore
1. User chooses a file and selects "Restore and reboot", confirming the prompt.
2. Browser reads the file text and POSTs it as the body to `/api/restore`.
3. `handleRestore` reads `server->arg("plain")` and calls
   `ConfigBackup::importJson`.
4. `importJson`:
   - Parses JSON. On parse error returns `ok = false` with an error string.
   - Reads `_meta.schemaVersion` into `fileVersion`. Sets `versionMismatch` when
     it differs from `CONFIG_SCHEMA_VERSION`. An absent value is treated as 0.
   - Calls `migrate(config, fileVersion)` when `fileVersion < CONFIG_SCHEMA_VERSION`.
   - For each recognized key present in `config`, calls the matching
     `ConfigStorage` setter, counting `applied`. Unknown keys count as `skipped`.
   - For image sources: `clearImageSources()`, then for each array element up to
     `MAX_IMAGE_SOURCES` add the URL and set enabled, duration, and transform.
   - Secrets: a secret field is applied only when present and non-empty, so a
     no-secrets backup never erases existing credentials.
   - Calls `configStorage.saveConfig()`.
5. `handleRestore` sends a JSON result, then if `ok` runs the reboot pattern
   (`delay(500)`, `crashLogger.saveBeforeReboot()`, `ESP.restart()`).

## Version management

- `CONFIG_SCHEMA_VERSION` starts at 1.
- Bump the constant when fields are added, renamed, removed, or change meaning.
- Lenient apply already handles additive changes: a new firmware reading an old
  file simply leaves new fields at defaults; an old firmware reading a new file
  ignores unknown keys.
- `migrate(JsonObject config, int fromVersion)` is the hook for non-additive
  changes. Example future use: if `cycleInterval` units change, the migration
  rewrites the value in the parsed document before the apply step. Version 1
  ships an empty `migrate`.
- The restore result reports `fileVersion` and `versionMismatch` so the UI can
  warn the user that a cross-version restore was applied on a best-effort basis.

## Error handling

- Empty or missing request body: `handleRestore` returns HTTP 400 with an error
  message, no reboot.
- JSON parse failure: `importJson` returns `ok = false`; handler returns HTTP 400.
- Missing `config` object: treated as parse failure.
- Out-of-range values: existing setters clamp or normalize, so malformed numbers
  cannot corrupt state.
- More than `MAX_IMAGE_SOURCES` entries: extra entries are skipped and counted.
- Partial application never reboots unless `ok` is true and at least the parse
  succeeded.

## Testing

Manual verification on device:
1. Configure non-default values across several sections.
2. Download a backup with secrets included; confirm the file contains the
   expected fields and the `_meta` block.
3. Download a backup with secrets excluded; confirm secret keys are absent.
4. Factory reset the device, complete WiFi setup, then restore the file and
   confirm the device reboots and comes back with the saved settings.
5. Restore a no-secrets backup and confirm existing credentials are retained.
6. Hand-edit the file to set `_meta.schemaVersion` to a higher number and confirm
   restore still applies recognized fields and reports a version mismatch.
7. Restore a file with an unknown extra key and confirm it is ignored.

Build verification: the sketch must compile for
`esp32:esp32:esp32p4` with the existing libraries.

## Out of scope

- Automatic cloud or scheduled backups.
- Encryption of the backup file. Secrets are stored in plaintext when included;
  the UI warns the user.
- Backup of crash logs or runtime state.
