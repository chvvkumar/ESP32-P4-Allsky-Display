# OTA Update Guide

Wireless firmware updates for ESP32-P4 AllSky Display using **safe A/B partitioning** with automatic rollback.

## 📋 Quick Reference

| Method | Best For | Access | Steps |
|--------|----------|--------|-------|
| **ElegantOTA** | End users, Production | Web browser | Compile → `http://[IP]:8080/update` → Upload `.bin` |
| **ArduinoOTA** | Developers, Rapid testing | Arduino IDE | Select network port → Upload |

**Features:** Auto-rollback on failure, config preserved, progress on display, no USB cable needed

---

## Method 1: ElegantOTA (Web Interface)

**Best for:** End users, one-time updates, no IDE required

### Step 1: Get Firmware Binary

**Option A - PowerShell Script:**
```powershell
.\compile-and-upload.ps1 -SkipUpload
```
Binary saved to Downloads folder.

**Option B - Arduino IDE:**
`Sketch → Export Compiled Binary` (`Ctrl+Alt+S`)
Look for: `ESP32-P4-Allsky-Display.ino.esp32p4.bin`

### Step 2: Upload via Web

1. Open `http://[device-ip]:8080/update` (find IP on display or router)
2. Select "Firmware" type
3. Drag & drop `.bin` file or click "Choose File"
4. Click "Update"
5. Wait 30-60 seconds - device shows progress and reboots automatically

Done! Verify at `http://[device-ip]:8080/`

---

## Method 2: ArduinoOTA (Arduino IDE)

**Best for:** Developers, rapid iteration, device already mounted

### Quick Steps

1. **Prerequisites:** Device on same WiFi network, Arduino IDE 1.8.19+, ESP32 core 3.3.4+
2. **Select Port:** `Tools → Port → esp32-allsky-display at [IP]` (look for 📶 WiFi icon)
3. **Upload:** Click Upload button (→)
4. **Wait:** 30-60 seconds, device shows progress and reboots

**If device doesn't appear:** Wait 60s, check WiFi connection, verify firewall allows port 3232

### Enable Password (Optional)

Edit `network_manager.cpp`:
```cpp
ArduinoOTA.setPassword("your-password");  // Uncomment this line
```
Recompile and upload once via USB.

---

## How It Works

### A/B Partition Scheme (32MB Flash)

```
OTA_0 (13MB) ←─── Active firmware
OTA_1 (13MB) ←─── Update target (inactive)
NVS (256KB)  ←─── Config (preserved)
Data (7MB)   ←─── Reserved
```

**Update Process:**
1. New firmware → Inactive partition (OTA_1)
2. Bootloader validates → Switches boot flag
3. Reboots → New firmware runs
4. **If boot fails** → Auto-rollback to OTA_0

**Your device cannot be bricked** - automatic recovery on failure

### Update Phases

| Phase | Duration | What Happens |
|-------|----------|--------------|
| Preparation | 1-2s | Display paused, watchdog extended, "OTA starting..." |
| Upload | 30-60s | Firmware written to inactive partition, progress shown |
| Verification | 5-10s | Checksum validation, boot flag updated |
| Reboot | 10-15s | Loads new firmware, validates boot success |
| Resume | - | Config restored, WiFi/MQTT reconnect, display resumes |

**Safety:** Watchdog protection, display pause, config preservation, automatic rollback

---

## Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| **Upload failed** | Verify partition scheme (13MB/7MB), reboot device, check network |
| **Can't access `/update`** | Check IP, verify port 8080 open, reboot device |
| **Stuck at 100%** | Normal - wait 10-15s for verification |
| **Device not in IDE port list** | Wait 60s, restart IDE, check same network, verify firewall allows port 3232 |
| **Connection failed** | Check WiFi signal, ensure network port selected (📶), ping device |
| **Upload timeout** | Improve WiFi signal, check power supply (2A min) |
| **Won't boot after update** | Auto-rollback activates (wait 30s), or connect USB for manual upload |
| **Config lost** | Rare NVS corruption - reconfigure via web interface |

### Quick Fixes

1. **Most issues:** Reboot device, verify network connection
2. **Firewall:** Allow ports 8080 (ElegantOTA), 3232 (ArduinoOTA), 5353 (mDNS)
3. **Rollback failed:** USB upload as last resort

---

## Security

**Default:** No password protection (suitable for home networks)

### Securing OTA

| Method | Steps | Use Case |
|--------|-------|----------|
| **Password** | Uncomment `ArduinoOTA.setPassword("...")` in `network_manager.cpp`, upload via USB once | Public networks |
| **Network Isolation** | IoT VLAN, firewall rules | Production |
| **Disable OTA** | Comment out OTA initialization in code | Locked-down deployments |

**Required Ports:** 8080 (web/ElegantOTA), 3232 (ArduinoOTA), 5353 (mDNS)

---

## Developer Reference

### OTA Manager API (`ota_manager.h`)

```cpp
// Status management
otaManager.setStatus(OTA_UPDATE_IN_PROGRESS, "Starting...");
OTAUpdateStatus status = otaManager.getStatus();  // IDLE, IN_PROGRESS, SUCCESS, FAILED

// Progress tracking (0-100%)
otaManager.setProgress(75);
otaManager.displayProgress("Firmware", 75);

// Main loop integration
void loop() {
    wifiManager.handleOTA();     // ArduinoOTA.handle()
    webConfig.handleClient();    // ElegantOTA.loop()
}
```

### Callbacks

**ElegantOTA** (`web_config.cpp`): `onStart()`, `onProgress()`, `onEnd()`  
**ArduinoOTA** (`network_manager.cpp`): `onStart()`, `onProgress()`, `onEnd()`, `onError()`

Key actions: Pause display, reset watchdog, show progress, resume display

### Resources

- [ElegantOTA GitHub](https://github.com/ayushsharma82/ElegantOTA)
- [ESP32 OTA Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ota.html)
- [Partition Tables Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)

---

## Summary

| Feature | ElegantOTA | ArduinoOTA |
|---------|------------|------------|
| Interface | Web browser | Arduino IDE |
| Best For | End users, one-time updates | Developers, rapid iteration |
| Password | ❌ Not built-in | ✅ Optional |
| Mobile | ✅ Yes | ❌ No |
| One-Click | ❌ Need .bin | ✅ Yes |

**Both methods:** Progress display, auto-rollback, config preserved
