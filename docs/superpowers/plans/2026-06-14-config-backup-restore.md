# Config Backup and Restore Implementation Plan

Date: 2026-06-14
Spec: docs/superpowers/specs/2026-06-14-config-backup-restore-design.md

## Overview

Add export and import of the full device configuration as a versioned JSON file,
with a reboot after restore. Work is split into four file-ownership lanes that
code against the fixed interface below so they can proceed in parallel.

## Fixed interface (all lanes code to this)

config_backup.h:

```cpp
#pragma once
#include <Arduino.h>

namespace ConfigBackup {
  struct RestoreResult {
    bool ok = false;
    String error;
    int fileVersion = 0;
    int applied = 0;
    int skipped = 0;
    bool versionMismatch = false;
    bool secretsIncluded = false;
  };

  String exportJson(bool includeSecrets);
  RestoreResult importJson(const String& body);
}
```

Endpoints:
- `GET /api/backup?secrets=0|1` returns the file as an attachment.
- `POST /api/restore` body is the raw file text; returns a JSON result and, on
  success, reboots.

`config.h` gains `#define CONFIG_SCHEMA_VERSION 1`.

## Lane A: Core module (owns config.h addition, config_backup.h, config_backup.cpp)

1. Add `#define CONFIG_SCHEMA_VERSION 1` to config.h in the IMAGE/SYSTEM area.
2. Create config_backup.h with the interface above.
3. Implement `exportJson(bool includeSecrets)`:
   - Build a `JsonDocument`.
   - Fill `_meta`: `schemaVersion` = `CONFIG_SCHEMA_VERSION`, `firmware` =
     `GIT_COMMIT_HASH` (include build_info.h), `deviceName`, `displayType`,
     `secretsIncluded`.
   - Fill `config` with every scalar field from `configStorage` getters, matching
     the key names in the spec format.
   - Build `imageSources` as an array of objects for indices
     `0 .. getImageSourceCount()-1`, each with url, enabled, duration, scaleX,
     scaleY, offsetX, offsetY, rotation.
   - Omit `wifiPassword`, `mqttPassword`, `haAccessToken` when `includeSecrets`
     is false.
   - `serializeJson` to a `String` and return it.
4. Implement `importJson(const String& body)`:
   - `deserializeJson`. On error or missing `config` object, return
     `{ ok=false, error=... }`.
   - Read `_meta.schemaVersion` into `fileVersion` (default 0). Set
     `versionMismatch = (fileVersion != CONFIG_SCHEMA_VERSION)`. Set
     `secretsIncluded` from `_meta.secretsIncluded`.
   - If `fileVersion < CONFIG_SCHEMA_VERSION`, call `migrate(config, fileVersion)`.
   - For each recognized scalar key present, call the matching setter and
     increment `applied`; for unrecognized keys increment `skipped`.
   - Secret fields: apply only when present and non-empty.
   - Image sources: `configStorage.clearImageSources()`, then iterate the array
     up to `MAX_IMAGE_SOURCES`; for each, `addImageSource(url)` then set enabled,
     duration, and the five transform fields by index. Skip and count extras.
   - Restore `currentImageIndex` and `imageSourceCount` consistency through the
     existing setters.
   - `configStorage.saveConfig()`.
   - Return the populated result.
5. Add `static void migrate(JsonObject config, int fromVersion)` as a no-op with
   a comment describing how to add future migrations.

Notes:
- Use ArduinoJson 7 API (`JsonDocument`, `doc["config"].to<JsonObject>()`, etc.).
- Guard array bounds with `MAX_IMAGE_SOURCES`.
- Do not reboot here.

## Lane B: Web transport (owns web_config.h, route block in web_config.cpp, handlers in web_config_api.cpp)

1. web_config.h: declare `void handleBackup();` and `void handleRestore();`.
2. web_config.cpp: register routes near the factory-reset and restart routes:
   - `server->on("/api/backup", HTTP_GET, [this]() { handleBackup(); });`
   - `server->on("/api/restore", HTTP_POST, [this]() { handleRestore(); });`
3. web_config_api.cpp: add `#include "config_backup.h"`.
4. `handleBackup()`:
   - `bool includeSecrets = server->hasArg("secrets") && server->arg("secrets") == "1";`
   - `String body = ConfigBackup::exportJson(includeSecrets);`
   - Set header `Content-Disposition: attachment; filename="allsky-config.json"`
     using `server->sendHeader(...)`, then `sendResponse(200, "application/json", body)`.
5. `handleRestore()`:
   - If no body (`!server->hasArg("plain")` or empty), respond HTTP 400 JSON error.
   - `ConfigBackup::RestoreResult r = ConfigBackup::importJson(server->arg("plain"));`
   - Build a JSON result with `status`, `message`, `applied`, `skipped`,
     `fileVersion`, `versionMismatch`.
   - If `!r.ok`, respond HTTP 400 and return without rebooting.
   - On success respond HTTP 200, then `delay(500); crashLogger.saveBeforeReboot();
     ESP.restart();` matching `handleRestart`.

## Lane C: UI (owns the System page generator in web_config_pages.cpp)

1. Locate the System settings page generator (the function behind
   `/config/system`, `handleAdvancedConfig` / `generateAdvancedPage`).
2. Add a "Backup and Restore" card consistent with existing card styling:
   - "Download backup" button and an "Include passwords and tokens" checkbox.
     The button triggers a navigation or fetch to
     `/api/backup?secrets=` + (checkbox ? `1` : `0`) that saves the file.
   - A file input and a "Restore and reboot" button. On click: confirm prompt,
     read the file with `FileReader.readAsText`, `fetch('/api/restore', { method:
     'POST', body: text })`, then show the returned message. Warn that the device
     will reboot. If `versionMismatch` is true, surface that in the message.
   - Note in the card that included secrets are stored in plaintext in the file.
3. Reuse existing toast or status helpers on the page if present.

## Lane D: Docs (owns docs/03_configuration.md and docs/developer/api_reference.md)

1. docs/03_configuration.md: add a "Backup and restore" section describing the
   download, the secrets checkbox, the restore-and-reboot flow, and the version
   behavior.
2. docs/developer/api_reference.md: document `GET /api/backup` and
   `POST /api/restore`, parameters, response shape, and the reboot side effect.
3. Follow the repository documentation style: no em dashes, no emojis, factual.

## Integration and verification (lead, after lanes merge)

1. Confirm config_backup.cpp compiles into the sketch (Arduino flat layout picks
   up new .cpp automatically).
2. Confirm the header/source pair check in CI passes (config_backup.h has a
   matching .cpp).
3. Compile for `esp32:esp32:esp32p4` locally if arduino-cli is available;
   otherwise rely on CI. Resolve any errors.
4. Review the round trip against the spec test list, focusing on secret omission,
   image-source reconstruction, and the version-mismatch report.

## Risks

- ESP32 WebServer body access: confirm `server->arg("plain")` holds the POST body
  for a non-form content type. If not, fall back to reading the body via an
  upload handler.
- JSON document size: roughly 6 to 8 KB. Allocate on heap (default ArduinoJson 7
  behavior). The device has PSRAM headroom.
- Field list drift: the export and import key lists must stay in sync with
  `ConfigStorage`. Keep both in config_backup.cpp so they are edited together.
