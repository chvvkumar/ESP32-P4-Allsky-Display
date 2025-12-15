# OTA Updates Guide

Complete guide to wireless firmware updates for ESP32-P4 AllSky Display using safe A/B partitioning with automatic rollback.

## Table of Contents
- [Quick Reference](#quick-reference)
- [Method 1: ElegantOTA (Web Interface)](#method-1-elegantota-web-interface)
- [Method 2: ArduinoOTA (Arduino IDE)](#method-2-arduinoota-arduino-ide)
- [How It Works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [Security](#security)
- [Developer Reference](#developer-reference)
- [Appendix: ESP32-C6 Co-Processor Updates](#appendix-esp32-c6-co-processor-updates)

---

## Quick Reference

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

---

## Appendix: ESP32-C6 Co-Processor Updates

This appendix documents the complete process of updating the ESP32-C6 slave firmware on the Waveshare ESP32-P4 Touch LCD using the ESP-Hosted OTA mechanism. The update is performed over SDIO from the ESP32-P4 host to the ESP32-C6 co-processor.

### Why Update the Co-Processor?

**Problem:** Version mismatch / RPC errors

If you see:
```
Version on Host is NEWER than version on co-processor
RPC requests sent by host may encounter timeout errors
E (5722) rpc_core: Response not received
```

**Root cause:** ESP-Hosted firmware mismatch between ESP32-P4 host and ESP32-C6 WiFi co-processor. The P4 has no built-in WiFi and relies on RPC (Remote Procedure Call) to the co-processor. Version mismatches cause the host's internal state ("I am connected") to get out of sync with the co-processor reality, leading to "zombie" connections and RPC timeouts.

**Solution:** ESP32-C6 slave firmware must be the same version or higher than the ESP32-P4 host firmware. Update the co-processor firmware using this guide.

### Overview

**Steps:**
- ✅ Built ESP-Hosted slave firmware (v2.7.2) for ESP32-C6
- ✅ Configured and built the host OTA example for ESP32-P4
- ✅ Performed successful OTA update over SDIO
- ✅ Upgraded slave firmware from v0.0.6 → v2.7.2
- ✅ Verified version checking and update prevention

**Document Version:** 1.0  
**Date:** December 11, 2025  
**ESP-IDF Version:** v5.5.1  
**ESP-Hosted Version:** v2.7.2  
**Tested Hardware:** [Waveshare 3.4" ESP32-P4 Touch LCD](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm)

### Additional Resources

- [ESP-Hosted MCU Documentation](https://github.com/espressif/esp-hosted-mcu)
- [ESP32-P4 Function EV Board Guide](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [OTA Update Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [ESPHome ESP32 Hosted Co-processor Update](https://esphome.io/components/update/esp32_hosted/)
- [ESP-Hosted Slave OTA Example](https://github.com/espressif/esp-hosted-mcu/tree/main/examples/host_performs_slave_ota)

### Prerequisites

**Hardware:**
- Waveshare ESP32-P4 Touch LCD (or similar ESP32-P4 board with on-board ESP32-C6)
- USB-C cable for programming/monitoring

**Software:**
- ESP-IDF v5.5.1 (installed and configured)
- Windows PowerShell (with ESP-IDF environment activated)
- Working `idf.py` command line tools

### Part 1: Building ESP32-C6 Slave Firmware

**Step 1: Create ESP-Hosted Slave Project**

```powershell
# Create a directory for your projects
mkdir F:\espp4c6
cd F:\espp4c6

# Create the slave project from ESP-Hosted example
# This downloads the example from the ESP Component Registry
idf.py create-project-from-example "espressif/esp_hosted^2.7.2:slave"
```

**Expected output:**
```
NOTICE: Example "slave" successfully downloaded to F:\espp4c6\slave
Done
```

**Step 2: Configure for ESP32-C6**

```powershell
cd slave

# Set target to ESP32-C6
idf.py set-target esp32c6
```

**Step 3: Build Slave Firmware**

```powershell
idf.py build
```

**Expected output:**
```
Building ESP-Hosted-MCU FW :: 2.7.2
...
network_adapter.bin binary size 0x11cec0 bytes
Project build complete.
```

**Step 4: Locate the Slave Binary**

The compiled slave firmware is located at:
```
F:\espp4c6\slave\build\network_adapter.bin
```

### Part 2: Setting Up Host OTA Example

**Step 1: Navigate to Host OTA Example**

```powershell
# Copy the example to your workspace (recommended)
cp -r C:\Espressif\frameworks\esp-idf-v5.5.1\examples\esp_hosted_mcu\host_performs_slave_ota F:\
cd F:\host_performs_slave_ota
```

**Step 2: Set Target to ESP32-P4**

```powershell
# Clean any existing build
idf.py fullclean

# Remove sdkconfig files
Remove-Item sdkconfig -ErrorAction SilentlyContinue
Remove-Item sdkconfig.old -ErrorAction SilentlyContinue

# Set target to ESP32-P4
idf.py set-target esp32p4
```

**Step 3: Prepare Slave Firmware for OTA**

```powershell
# For Partition OTA method (recommended)
New-Item -Path "components\ota_partition\slave_fw_bin" -ItemType Directory -Force

# Copy the slave firmware binary
Copy-Item "F:\espp4c6\slave\build\network_adapter.bin" -Destination "components\ota_partition\slave_fw_bin\"
```

**Step 4: Configure OTA Method**

```powershell
idf.py menuconfig
```

**Navigate to:**
```
Component config --->
    ESP-Hosted Slave OTA Configuration --->
        OTA Method --->
            (X) Partition OTA   ← SELECT THIS
            
        # Disable version matching to force update
        [ ] Skip OTA if slave firmware versions match  ← UNCHECK
```

**Save and exit (S, then Enter)**

### Part 3: Building and Flashing

**Step 1: Build the Host Firmware**

```powershell
idf.py -p COM3 build
```

**Step 2: Flash Everything Manually**

Due to a bug in the flash script, flash manually with esptool:

```powershell
esptool.py --chip esp32p4 -p COM3 -b 460800 `
  --before=default_reset --after=hard_reset `
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB `
  0x2000 build/bootloader/bootloader.bin `
  0x10000 build/host_performs_slave_ota.bin `
  0x8000 build/partition_table/partition-table.bin `
  0xd000 build/ota_data_initial.bin `
  0x5F0000 components/ota_partition/slave_fw_bin/network_adapter.bin `
  --force
```

**Note:** The `--force` flag is required because the slave firmware is for ESP32-C6, not ESP32-P4.

### Part 4: Monitoring the OTA Update

**Step 1: Start Monitor**

```powershell
idf.py -p COM3 monitor
```

**Step 2: Watch the OTA Process**

**First boot - OTA update in progress:**

```
I (2279) host_performs_slave_ota: ESP-Hosted initialized successfully
I (2279) host_performs_slave_ota: Using Partition OTA method
I (2285) ota_partition: Starting Partition OTA from partition: slave_fw
I (2299) ota_partition: Found app description: version='2.7.2', project_name='network_adapter'
I (2344) ota_partition: Firmware verified - Size: 1167041 bytes, Version: 2.7.2
I (2359) ota_partition: Proceeding with OTA - Firmware size: 1167041 bytes
I (21856) ota_partition: Partition OTA completed successfully - Sent 1167041 bytes
I (21856) host_performs_slave_ota: OTA completed successfully
```

✅ **OTA Update Successful!**

**Step 3: Verify the Update**

**Press RESET button or reboot:**

```powershell
# Exit monitor (Ctrl+])
# Restart monitor
idf.py -p COM3 monitor
```

**Second boot - verification:**

```
I (2376) ota_partition: Current slave firmware version: 2.7.2
I (2377) ota_partition: New slave firmware version: 2.7.2
W (2377) ota_partition: Current slave firmware version (2.7.2) is the same as new version (2.7.2). Skipping OTA.
I (2386) host_performs_slave_ota: OTA not required
```

✅ **Version Check Working - Slave Now Running v2.7.2!**

### Understanding the OTA Methods

**Partition OTA (Used in This Guide):**
- **Best for:** Production, most reliable
- **How it works:** Slave firmware embedded directly into flash partition on host, host reads and sends to slave
- **Pros:** Fast, reliable, no filesystem overhead
- **Partition Table:** `slave_fw, data, 0x40, 0x5F0000, 0x200000` (2MB for slave firmware)

**LittleFS OTA (Alternative):**
- **Best for:** Dynamic updates, multiple firmware files
- **How it works:** Slave firmware stored as files in LittleFS filesystem
- **Pros:** More flexible but slightly more complex

**HTTPS OTA (Alternative):**
- **Best for:** Remote/internet updates
- **How it works:** Downloads slave firmware from web server
- **Requires:** WiFi connectivity

### Build Workflow

**Two-Stage Build Process:**

| Step | Folder | Target Chip | Command | Output | Purpose |
|------|--------|-------------|---------|--------|---------|
| **1** | `F:\espp4c6\slave\` | ESP32-C6 | `idf.py build` | `network_adapter.bin` | Standalone slave firmware (WiFi/BT adapter) |
| **2** | `F:\host_performs_slave_ota\` | ESP32-P4 | `idf.py build` | Host firmware + embedded slave | OTA updater that flashes slave via SDIO |

**Data Flow:**
```
┌────────────────┐
│ Step 1: Slave  │  idf.py build (ESP32-C6)
│ F:\espp4c6\    │  → network_adapter.bin (1.17 MB)
└────────┬───────┘
         │ 
         │ COPY
         ▼
┌────────────────────────────────────────────────────────┐
│ Step 2: Host                                           │
│ F:\host_performs_slave_ota\                            │
│   components/ota_partition/slave_fw_bin/               │
│     network_adapter.bin ← PASTE HERE                   │
│                                                         │
│ idf.py build (ESP32-P4)                                │
│   → Embeds slave firmware at partition offset 0x5F0000 │
│   → Creates complete flash image                       │
└─────────────────────────────────────────────────────────┘
```

### Troubleshooting

**Issue: "No .bin files found"**

**Solution:**
- Ensure slave firmware is in correct directory: `components/ota_partition/slave_fw_bin/`
- File can be named anything.bin (e.g., `network_adapter.bin`)

**Issue: Build fails with "write-flash" error**

**Solution:**
- Flash manually using esptool command (see Part 3, Step 2)
- This is a known bug in the flash script

**Issue: "Expected chip id 18 but value was 13"**

**Solution:**
- This is expected! Slave firmware is for ESP32-C6 (13), host is ESP32-P4 (18)
- Use `--force` flag with esptool

**Issue: OTA activation fails**

**Symptoms:**
```
E (26858) host_performs_slave_ota: Failed to activate OTA: ESP_FAIL
```

**Solution:**
- This timeout is normal - slave is writing firmware to flash
- Simply reset the board and verify the update worked
- Check if version changed on next boot

### Complete Command Reference

**Quick Start (All-in-One):**

```powershell
# 1. Build slave firmware
cd F:\espp4c6\slave
idf.py set-target esp32c6
idf.py build

# 2. Setup host project
cd F:\host_performs_slave_ota
idf.py fullclean
Remove-Item sdkconfig*
idf.py set-target esp32p4

# 3. Copy slave firmware
New-Item -Path "components\ota_partition\slave_fw_bin" -ItemType Directory -Force
Copy-Item "F:\espp4c6\slave\build\network_adapter.bin" `
    -Destination "components\ota_partition\slave_fw_bin\"

# 4. Configure (select Partition OTA in menuconfig)
idf.py menuconfig

# 5. Build host
idf.py build

# 6. Flash everything
esptool.py --chip esp32p4 -p COM3 -b 460800 `
    --before=default_reset --after=hard_reset `
    write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB `
    0x2000 build/bootloader/bootloader.bin `
    0x10000 build/host_performs_slave_ota.bin `
    0x8000 build/partition_table/partition-table.bin `
    0xd000 build/ota_data_initial.bin `
    0x5F0000 components/ota_partition/slave_fw_bin/network_adapter.bin `
    --force

# 7. Monitor
idf.py -p COM3 monitor
```

### Success Indicators

✅ **Build Success:**
- `network_adapter.bin` created for slave (ESP32-C6)
- `host_performs_slave_ota.bin` created for host (ESP32-P4)
- Both binaries built without errors

✅ **Flash Success:**
- All partitions flashed with "Hash of data verified"
- Board resets and boots

✅ **OTA Success:**
- Log shows "Partition OTA completed successfully"
- All bytes transferred (e.g., "Sent 1167041 bytes")

✅ **Verification Success:**
- Second boot shows matching versions (both 2.7.2)
- "Skipping OTA" message appears
- No version mismatch warnings

### Summary

This process successfully demonstrates:

1. **Cross-chip OTA**: Updating a different chip (ESP32-C6) from the host (ESP32-P4)
2. **SDIO transport**: Using high-speed SDIO for firmware transfer
3. **Version management**: Automatic version checking and update prevention
4. **Partition-based OTA**: Most reliable method for embedded firmware storage

The Waveshare ESP32-P4 Touch LCD now has both chips running updated ESP-Hosted firmware with version compatibility! 🎉

---

## Next Steps

- [Configuration Guide](03_configuration.md) - Detailed configuration options
- [Features Guide](04_features.md) - Explore all system capabilities
- [Troubleshooting](06_troubleshooting.md) - Common issues and solutions
- [Hardware Guide](01_hardware.md) - Hardware specifications and setup
- [Installation Guide](02_installation.md) - Step-by-step setup
