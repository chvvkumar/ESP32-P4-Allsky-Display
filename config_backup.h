#pragma once
#include <Arduino.h>

// Config backup/restore module. Single owner of the mapping between
// ConfigStorage state and the versioned JSON backup document.
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

  // Serialize the full device configuration to JSON. When includeSecrets is
  // false the wifiPassword, mqttPassword, and haAccessToken keys are omitted.
  String exportJson(bool includeSecrets);

  // Parse a backup document and apply recognized fields through ConfigStorage
  // setters, then saveConfig(). Does not reboot; the caller handles that.
  RestoreResult importJson(const String& body);
}
