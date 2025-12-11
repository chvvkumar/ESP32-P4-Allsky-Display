# ESP-Hosted Slave OTA Update Guide for Waveshare ESP32-P4

## Overview

This guide documents the complete process of updating the ESP32-C6 slave firmware on the Waveshare ESP32-P4 Touch LCD using the ESP-Hosted OTA mechanism. The update is performed over SDIO from the ESP32-P4 host to the ESP32-C6 co-processor.

**Steps:**
- ✅ Built ESP-Hosted slave firmware (v2.7.2) for ESP32-C6
- ✅ Configured and built the host OTA example for ESP32-P4
- ✅ Performed successful OTA update over SDIO
- ✅ Upgraded slave firmware from v0.0.6 → v2.7.2
- ✅ Verified version checking and update prevention

---

**Document Version:** 1.0  
**Date:** December 11, 2025  
**ESP-IDF Version:** v5.5.1  
**ESP-Hosted Version:** v2.7.2  
**Tested Hardware:** [Waveshare 3.4" ESP32-P4 Touch LCD](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-3.4c.htm)

---

## Additional Resources

- [ESP-Hosted MCU Documentation](https://github.com/espressif/esp-hosted-mcu)
- [ESP32-P4 Function EV Board Guide](https://github.com/espressif/esp-hosted-mcu/blob/main/docs/esp32_p4_function_ev_board.md)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [OTA Update Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html)
- [ESPHome ESP32 Hosted Co-processor Update](https://esphome.io/components/update/esp32_hosted/)
- [ESP-Hosted Slave OTA Example](https://github.com/espressif/esp-hosted-mcu/tree/main/examples/host_performs_slave_ota)

---

## Prerequisites

### Hardware
- Waveshare ESP32-P4 Touch LCD (or similar ESP32-P4 board with on-board ESP32-C6)
- USB-C cable for programming/monitoring

### Software
- ESP-IDF v5.5.1 (installed and configured)
- Windows PowerShell (with ESP-IDF environment activated)
- Working `idf.py` command line tools

---

## Part 0: ESP-IDF Installation (If Not Already Installed)

### Option A: Use Pre-installed ESP-IDF (Recommended)

If you already have ESP-IDF v5.5.1 installed (e.g., via the Windows installer), you can skip this section.

**Activate the ESP-IDF environment:**

```powershell
# Using the ESP-IDF PowerShell shortcut or:
C:\Espressif\frameworks\esp-idf-v5.5.1\export.ps1
```

### Option B: Clone ESP-IDF from GitHub

If you need to install ESP-IDF manually:

```powershell
# Create a directory for ESP-IDF
mkdir F:\espp4c6
cd F:\espp4c6

# Clone ESP-IDF (example showing v6.0 branch)
git clone -b release/v6.0 --recursive https://github.com/espressif/esp-idf.git

# Or clone the specific version used in this guide
git clone -b release/v5.5 --recursive https://github.com/espressif/esp-idf.git

# Navigate to ESP-IDF directory
cd esp-idf

# Install ESP-IDF tools (this may take some time)
./install.sh esp32p4 esp32c6

# Activate ESP-IDF environment
./export.bat  # For Windows CMD
# or
./export.ps1  # For PowerShell
```

**Note:** This guide was tested with ESP-IDF v5.5.1. The git clone example above shows v6.0, but v5.5.1 is recommended for stability with this workflow.

**Verify installation:**

```powershell
idf.py --version
```

Expected output should show ESP-IDF v5.5.1 or compatible version.

---

## Part 1: Building ESP32-C6 Slave Firmware

### Step 1: Create ESP-Hosted Slave Project

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

**Note:** This command downloads the slave firmware example from the [ESP Component Registry](https://components.espressif.com/components/espressif/esp_hosted), not directly from GitHub. The component manager handles all dependencies automatically.

### Step 2: Configure for ESP32-C6

```powershell
cd slave

# Set target to ESP32-C6
idf.py set-target esp32c6
```

**Expected output:**
```
Set Target to: esp32c6, new sdkconfig will be created.
Building ESP-IDF components for target esp32c6
```

### Step 3: Configure Slave Settings (Optional)

```powershell
# Open menuconfig to customize settings
idf.py menuconfig
```

**Key configuration options:**
- Transport: SDIO (default for ESP32-C6 on Waveshare ESP32-P4 Touch LCD)
- Bluetooth: Configure as needed
- Wi-Fi settings: Default settings work

### Step 4: Build Slave Firmware

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

### Step 5: Locate the Slave Binary

The compiled slave firmware is located at:
```
F:\espp4c6\slave\build\network_adapter.bin
```

**This binary will be embedded in the host firmware for OTA updates.**

---

## Part 2: Setting Up Host OTA Example

### Step 1: Navigate to Host OTA Example

The `host_performs_slave_ota` example is included in ESP-IDF v5.5.1 installation:

```powershell
# Option A: Work directly in the ESP-IDF examples directory
cd C:\Espressif\frameworks\esp-idf-v5.5.1\examples\esp_hosted_mcu\host_performs_slave_ota

# Option B: Copy the example to your workspace (recommended)
# This allows you to modify without affecting the original
cp -r C:\Espressif\frameworks\esp-idf-v5.5.1\examples\esp_hosted_mcu\host_performs_slave_ota F:\
cd F:\host_performs_slave_ota
```

**Note:** The ESP-Hosted component and dependencies are automatically downloaded by the ESP-IDF component manager during the build process. You don't need to manually clone the ESP-Hosted repository.

### Step 2: Set Target to ESP32-P4

```powershell
# Clean any existing build
idf.py fullclean

# Remove sdkconfig files
Remove-Item sdkconfig -ErrorAction SilentlyContinue
Remove-Item sdkconfig.old -ErrorAction SilentlyContinue

# Set target to ESP32-P4
idf.py set-target esp32p4
```

**During this step, you'll see the component manager download dependencies:**

```
NOTICE: Processing 9 dependencies:
NOTICE: [1/9] cmd_nvs (*)
NOTICE: [2/9] cmd_system (*)
NOTICE: [3/9] espressif/eppp_link (1.1.4)
NOTICE: [4/9] espressif/esp_hosted (2.7.2)         ← ESP-Hosted automatically downloaded
NOTICE: [5/9] espressif/esp_serial_slave_link (1.1.2)
NOTICE: [6/9] espressif/esp_wifi_remote (1.2.2)
NOTICE: [7/9] espressif/wifi_remote_over_eppp (0.2.1)
NOTICE: [8/9] joltwallet/littlefs (1.20.3)
NOTICE: [9/9] idf (5.5.1)
```

These components are stored in `managed_components/` directory and don't require manual git cloning.

### Step 3: Prepare Slave Firmware for OTA

**Create the directory structure:**

```powershell
# For Partition OTA method (recommended)
New-Item -Path "components\ota_partition\slave_fw_bin" -ItemType Directory -Force

# Copy the slave firmware binary
Copy-Item "F:\espp4c6\slave\build\network_adapter.bin" -Destination "components\ota_partition\slave_fw_bin\"
```

**Directory structure should look like:**

```
📁 Project Workspace (Two-Stage Build Process)
│
│   ┌─────────────────────────────────┐
│   │ STEP 1: Build Slave Firmware   │
│   │ Target: ESP32-C6                │
│   │ Purpose: Network adapter only   │
│   └─────────────────────────────────┘
├── 📁 F:\espp4c6\                          
│   └── slave/                              
│       ├── main/
│       │   ├── app_main.c                 # Slave application entry point
│       │   └── CMakeLists.txt
│       ├── build/
│       │   └── network_adapter.bin        # ✅ OUTPUT: Slave firmware (v2.7.2, 1.17MB)
│       ├── sdkconfig                      #    Built with: idf.py set-target esp32c6
│       └── CMakeLists.txt                 #                idf.py build
│                                          #
│                                          # ⬇️ COPY THIS FILE TO HOST PROJECT ⬇️
│                                          #
│   ┌─────────────────────────────────┐
│   │ STEP 2: Build Host+Slave Bundle │
│   │ Target: ESP32-P4                │
│   │ Purpose: OTA updater + embedded │
│   │          slave firmware         │
│   └─────────────────────────────────┘
└── 📁 F:\host_performs_slave_ota\         
    │
    ├── 📁 main/                           # Host application
    │   ├── app_main.c                     # Host OTA logic entry point
    │   └── CMakeLists.txt
    │
    ├── 📁 components/                      # OTA method implementations
    │   ├── ota_partition/                 # ✅ Partition OTA (RECOMMENDED)
    │   │   ├── slave_fw_bin/              # ⬅️ CREATE THIS & COPY network_adapter.bin HERE
    │   │   │   └── network_adapter.bin    # ✅ INPUT: Copied from slave/build/
    │   │   ├── ota_partition.c            # Partition OTA implementation
    │   │   ├── ota_partition.h
    │   │   └── CMakeLists.txt             # 🔧 Embeds .bin into flash partition
    │   │
    │   ├── ota_littlefs/                  # LittleFS OTA (alternative method)
    │   │   ├── slave_fw_bin/              # For filesystem-based OTA
    │   │   ├── ota_littlefs.c
    │   │   ├── ota_littlefs.h
    │   │   └── CMakeLists.txt
    │   │
    │   └── ota_https/                     # HTTPS OTA (alternative method)
    │       ├── ota_https.c
    │       ├── ota_https.h
    │       └── CMakeLists.txt
    │
    ├── 📁 managed_components/              # Auto-downloaded by ESP component manager
    │   ├── espressif__esp_hosted/         # ESP-Hosted v2.7.2 (SDIO transport)
    │   ├── espressif__eppp_link/
    │   ├── espressif__esp_wifi_remote/
    │   └── joltwallet__littlefs/
    │
    ├── 📁 build/                           # ✅ OUTPUT: Complete flash image (ESP32-P4)
    │   ├── bootloader/
    │   │   └── bootloader.bin             # ESP32-P4 bootloader
    │   ├── partition_table/
    │   │   └── partition-table.bin        # Flash layout with slave_fw partition
    │   ├── host_performs_slave_ota.bin    # Main host firmware
    │   ├── host_performs_slave_ota.elf    # Debug symbols (for addr2line)
    │   └── ota_data_initial.bin           # OTA data partition
    │                                      # Built with: idf.py set-target esp32p4
    │                                      #             idf.py build
    ├── sdkconfig                          # ESP32-P4 configuration
    ├── partitions.csv                     # Custom partition table (includes slave_fw)
    └── CMakeLists.txt                     # Top-level build config

```

**Build Order & Purpose:**

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

**Key Files:**
- **Slave Firmware Source:** `F:\espp4c6\slave\build\network_adapter.bin` (1.17 MB, v2.7.2)
- **Host Firmware Destination:** `F:\host_performs_slave_ota\components\ota_partition\slave_fw_bin\`
- **Final Flash Binary:** Slave firmware embedded at partition offset `0x5F0000` (6 MB mark)

**Complete Directory Structure After Setup:**
```
host_performs_slave_ota/
├── components/
│   ├── ota_partition/
│   │   ├── slave_fw_bin/              ← Directory you create
│   │   │   └── network_adapter.bin    ← Your slave firmware (copy from slave build)
│   │   ├── CMakeLists.txt
│   │   ├── ota_partition.c
│   │   └── ota_partition.h
│   ├── ota_littlefs/
│   └── ota_https/
├── main/
└── CMakeLists.txt
```

### Step 4: Configure OTA Method

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

---

## Part 3: Building and Flashing

### Step 1: Build the Host Firmware

```powershell
idf.py -p COM3 build
```

**Expected output:**
```
✅ Partition OTA: Found slave_fw partition at offset: 0x5F0000
✅ Partition OTA: Found firmware files for dynamic selection
...
host_performs_slave_ota.bin binary size 0x8efb0 bytes
```

### Step 2: Flash Everything Manually

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

**Expected output:**
```
Chip is ESP32-P4 (revision v1.0)
...
Unexpected chip id in image. Expected 18 but value was 13.
...
Hash of data verified.
Hard resetting via RTS pin...
```

---

## Part 4: Monitoring the OTA Update

### Step 1: Start Monitor

```powershell
idf.py -p COM3 monitor
```

### Step 2: Watch the OTA Process

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

### Step 3: Verify the Update

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

---

## Understanding the OTA Methods

### Partition OTA (Used in This Guide)
**Best for:** Production, most reliable

**How it works:**
- Slave firmware is embedded directly into a flash partition on the host
- Host reads firmware from partition and sends it to slave
- Fast and reliable, no filesystem overhead
- Firmware flashed at build time

**Partition Table:**
```
slave_fw, data, 0x40, 0x5F0000, 0x200000  # 2MB for slave firmware
```

### LittleFS OTA (Alternative)
**Best for:** Dynamic updates, multiple firmware files

**How it works:**
- Slave firmware stored as files in LittleFS filesystem
- Build system creates filesystem image
- More flexible but slightly more complex

### HTTPS OTA (Alternative)
**Best for:** Remote/internet updates

**How it works:**
- Downloads slave firmware from web server
- Requires Wi-Fi connectivity
- Good for field updates

---

## Troubleshooting

### Issue: "No .bin files found"

**Solution:**
- Ensure slave firmware is in correct directory: `components/ota_partition/slave_fw_bin/`
- File can be named anything.bin (e.g., `network_adapter.bin`)

### Issue: Build fails with "write-flash" error

**Solution:**
- Flash manually using esptool command (see Part 3, Step 2)
- This is a known bug in the flash script

### Issue: "Expected chip id 18 but value was 13"

**Solution:**
- This is expected! Slave firmware is for ESP32-C6 (13), host is ESP32-P4 (18)
- Use `--force` flag with esptool

### Issue: OTA activation fails

**Symptoms:**
```
E (26858) host_performs_slave_ota: Failed to activate OTA: ESP_FAIL
```

**Solution:**
- This timeout is normal - slave is writing firmware to flash
- Simply reset the board and verify the update worked
- Check if version changed on next boot

### Issue: Version mismatch warning persists

**Symptoms:**
```
W (2127) transport: Version mismatch: Host [2.7.0] > Co-proc [0.0.0]
```

**Solution:**
- Ensure slave firmware was actually updated
- Check that version is read correctly: look for logs showing actual version
- If RPC timeouts occur, the version read may fail but OTA still succeeds

---

## Complete Command Reference

### Quick Start (All-in-One)

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

---

## Success Indicators

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

---

## Summary

This process successfully demonstrates:

1. **Cross-chip OTA**: Updating a different chip (ESP32-C6) from the host (ESP32-P4)
2. **SDIO transport**: Using high-speed SDIO for firmware transfer
3. **Version management**: Automatic version checking and update prevention
4. **Partition-based OTA**: Most reliable method for embedded firmware storage

The Waveshare ESP32-P4 Touch LCD now has both chips running updated ESP-Hosted firmware with version compatibility! 🎉

---

## Appendix A: Key File Locations

| Component | Path | Description |
|-----------|------|-------------|
| Slave firmware binary | `F:\espp4c6\slave\build\network_adapter.bin` | ESP32-C6 compiled firmware |
| Host firmware binary | `F:\host_performs_slave_ota\build\host_performs_slave_ota.bin` | ESP32-P4 compiled firmware |
| Slave firmware for OTA | `F:\host_performs_slave_ota\components\ota_partition\slave_fw_bin\network_adapter.bin` | Copy of slave firmware embedded in host |
| Partition table | `F:\host_performs_slave_ota\build\partition_table\partition-table.bin` | Flash partition layout |
| Bootloader | `F:\host_performs_slave_ota\build\bootloader\bootloader.bin` | ESP32-P4 bootloader |

---

**Document Version:** 1.0  
**Date:** December 11, 2025  
**ESP-IDF Version:** v5.5.1  
**ESP-Hosted Version:** v2.7.2  
**Tested Hardware:** ESP32-P4-Function-EV-Board

---

## Appendix B: Repositories and Component Sources

### ESP-IDF Framework

**Official Repository:**
```bash
# ESP-IDF v5.5 branch (used in this guide)
git clone -b release/v5.5 --recursive https://github.com/espressif/esp-idf.git

# ESP-IDF v6.0 branch (newer, but not tested with this guide)
git clone -b release/v6.0 --recursive https://github.com/espressif/esp-idf.git
```

**URL:** https://github.com/espressif/esp-idf

### ESP-Hosted-MCU

**Official Repository:**
```bash
# Main repository (for reference, not needed for this workflow)
git clone https://github.com/espressif/esp-hosted-mcu.git
```

**URL:** https://github.com/espressif/esp-hosted-mcu

**Component Registry:** https://components.espressif.com/components/espressif/esp_hosted

**Note:** When using `idf.py create-project-from-example`, the component is automatically downloaded from the ESP Component Registry, not cloned from GitHub.

### Submodules Included in ESP-IDF

When you clone ESP-IDF with `--recursive`, it automatically clones these submodules:

```
Submodule 'components/bootloader/subproject/components/micro-ecc/micro-ecc' 
  → https://github.com/kmackay/micro-ecc.git

Submodule 'components/bt/controller/lib_esp32' 
  → https://github.com/espressif/esp32-bt-lib.git

Submodule 'components/bt/controller/lib_esp32c6/esp32c6-bt-lib' 
  → https://github.com/espressif/esp32c6-bt-lib.git

Submodule 'components/bt/host/nimble/nimble' 
  → https://github.com/espressif/esp-nimble.git

Submodule 'components/cmock/CMock' 
  → https://github.com/ThrowTheSwitch/CMock.git

Submodule 'components/esp_wifi/lib' 
  → https://github.com/espressif/esp32-wifi-lib.git

Submodule 'components/lwip/lwip' 
  → https://github.com/espressif/esp-lwip.git

Submodule 'components/mbedtls/mbedtls' 
  → https://github.com/espressif/mbedtls.git

... and more (see full list in .gitmodules)
```

### Component Manager Dependencies

These components are automatically downloaded during build from the ESP Component Registry:

| Component | Version | Registry URL |
|-----------|---------|--------------|
| espressif/esp_hosted | 2.7.2 | https://components.espressif.com/components/espressif/esp_hosted |
| espressif/esp_wifi_remote | 1.2.2 | https://components.espressif.com/components/espressif/esp_wifi_remote |
| espressif/eppp_link | 1.1.4 | https://components.espressif.com/components/espressif/eppp_link |
| espressif/esp_serial_slave_link | 1.1.2 | https://components.espressif.com/components/espressif/esp_serial_slave_link |
| espressif/wifi_remote_over_eppp | 0.2.1 | https://components.espressif.com/components/espressif/wifi_remote_over_eppp |
| joltwallet/littlefs | 1.20.3 | https://components.espressif.com/components/joltwallet/littlefs |

### Manual Installation (Not Required)

If you want to manually clone ESP-Hosted-MCU for development or reference:

```powershell
# Clone the main repository
git clone https://github.com/espressif/esp-hosted-mcu.git
cd esp-hosted-mcu

# The slave firmware is in:
# esp-hosted-mcu/slave/

# The host examples are in:
# esp-hosted-mcu/host/
```

However, for the OTA update workflow in this guide, **manual cloning is not necessary** as everything is handled by the component manager and ESP-IDF's built-in examples.