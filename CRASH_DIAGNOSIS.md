# Crash Diagnosis Guide

## Understanding PANIC/EXCEPTION Crashes

When you see `PANIC/EXCEPTION` in the crash logs, it means the ESP32 encountered an unhandled exception such as:
- **Null pointer dereference** - Accessing memory at address 0
- **Illegal instruction** - Trying to execute invalid code
- **Memory access violation** - Reading/writing to protected or unmapped memory
- **Stack overflow** - Function call stack exceeded available space
- **Heap corruption** - Memory management structures damaged
- **Division by zero** - Arithmetic exception

## Enhanced Crash Information (After Update)

With the latest firmware update, crash logs now include:

### 1. **System State at Crash**
```
[2025-12-08 13:27:03] [CrashLogger] !!! CRASH DETECTED !!! Reset reason: PANIC/EXCEPTION
[2025-12-08 13:27:03] [CrashLogger] CPU Freq: 360 MHz, XTAL: 40 MHz
[2025-12-08 13:27:03] [CrashLogger] Heap at crash: 317804 bytes free, Largest block: 118784 bytes
[2025-12-08 13:27:03] [CrashLogger] PSRAM at crash: 25165824 bytes free of 33554432 total
```
**All logs now include NTP timestamps** showing the exact date/time the event occurred.

### 2. **Backtrace (Call Stack)**
```
[2025-12-08 13:27:03] [CrashLogger] Backtrace (if available):
  [0] PC: 0x4200ABCD SP: 0x3FCE8000
  [1] PC: 0x4200DEF0 SP: 0x3FCE8020
  [2] PC: 0x42011234 SP: 0x3FCE8040
```

The Program Counter (PC) addresses show which functions were executing when the crash occurred.

### 3. **NVS Log Timestamps**
```
===== CRASH LOGGER DUMP =====
Current Time: 2025-12-08 19:30:45
Boot Count: 3
Session Uptime: 42514 ms

--- NVS Logs (Preserved from Previous Boot) ---
[CrashLogger] NVS logs from boot #2 (saved at 2025-12-08 19:24:12):
```

NVS logs show exactly when they were saved, making it easy to determine crash age.

## How to Diagnose Crashes

### Method 1: Check Enhanced Logs (Easiest)

1. Open **Console** page in web interface (auto-connects now)
2. Look for crash section with:
   - Reset reason
   - Memory state (heap/PSRAM)
   - Backtrace addresses
3. Note the PC addresses from backtrace

### Method 2: Decode Backtrace with addr2line

Use the GCC toolchain to convert addresses to source code locations:

```powershell
# Find your ESP32 toolchain (usually in Arduino packages)
$TOOLCHAIN = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\xtensa-esp32-elf-gcc\*\bin"

# Get the .elf file from your build (contains debug symbols)
$ELF_FILE = ".\build\ESP32-P4-Allsky-Display.ino.elf"

# Decode a crash address
& "$TOOLCHAIN\xtensa-esp32-elf-addr2line.exe" -e $ELF_FILE -f -p 0x4200ABCD
```

This will output something like:
```
downloadImage() at C:/Users/Kumar/git/ESP32-P4-Allsky-Display/ESP32-P4-Allsky-Display.ino:1234
```

### Method 3: Arduino IDE Exception Decoder

1. Install **ESP Exception Decoder** from Tools → Manage Libraries
2. Tools → ESP Exception Decoder
3. Paste the entire crash dump from Serial Monitor
4. It automatically decodes all addresses to function names and line numbers

### Method 4: Serial Monitor During Crash

If you have USB connected during a crash, the ESP32 outputs detailed register dumps:

```
Guru Meditation Error: Core 0 panic'ed (LoadProhibited). Exception was unhandled.
Core 0 register dump:
PC      : 0x4200abcd  PS      : 0x00060030  A0      : 0x800d1234  A1      : 0x3fce8000
...
Backtrace: 0x4200abcd:0x3fce8000 0x800d1234:0x3fce8020 ...
```

Copy this entire output and use the Exception Decoder (Method 3).

## Common Crash Patterns

### Pattern: Crash During Image Download
**Symptoms:**
- `PANIC/EXCEPTION` after "Downloading image data..."
- Memory fragmentation warnings before crash

**Likely Causes:**
- Network buffer allocation failure
- HTTP client timeout during malloc
- JPEG decoder buffer overflow

**Solutions:**
- Check if image is too large (>512×512 pixels may exceed buffer)
- Verify image URL is accessible
- Check free heap before download

### Pattern: Crash During Display Update
**Symptoms:**
- Crash after "Rendering image"
- PPA-related messages before crash

**Likely Causes:**
- DMA buffer alignment issue
- Cache coherency problem
- Invalid image dimensions

**Solutions:**
- Verify image dimensions match expectations
- Check PPA buffer sizes in config.h
- Ensure images are valid JPEG format

### Pattern: Crash on Boot/WiFi Setup
**Symptoms:**
- Crash during captive portal
- Repeating boot loops

**Likely Causes:**
- NVS corruption
- WiFi credential memory issue
- Watchdog timeout during connection

**Solutions:**
- Factory reset via web interface
- Clear NVS: Tools → ESP32 Sketch Data Upload
- Check WiFi signal strength

### Pattern: Crash During MQTT Operations
**Symptoms:**
- Crash after MQTT message received
- Home Assistant command processing

**Likely Causes:**
- Payload buffer overflow
- String allocation failure
- JSON parsing error

**Solutions:**
- Check MQTT message sizes (<2KB)
- Verify JSON format in HA commands
- Monitor heap before MQTT operations

## Preventive Measures

### 1. Watchdog Protection
The system automatically resets watchdog every 1 second. Long operations call:
```cpp
systemMonitor.forceResetWatchdog();
```

### 2. Memory Monitoring
Check heap/PSRAM regularly:
```cpp
ESP.getFreeHeap()      // Heap available
ESP.getMaxAllocHeap()  // Largest allocatable block
ESP.getFreePsram()     // PSRAM available
```

### 3. Bounds Checking
Always validate array indices and pointer accesses:
```cpp
if (index >= 0 && index < array_size) {
    // Safe to access
}
```

### 4. Buffer Overflow Protection
Never exceed allocated buffer sizes:
```cpp
size_t bytesRead = http.getStream().readBytes(buffer, min(available, bufferSize));
```

## Testing for Crashes

### Manual Crash Test
To verify crash logging works, you can temporarily add:
```cpp
// In setup() or loop() - REMOVE AFTER TESTING
int* p = nullptr;
*p = 42;  // Intentional null pointer crash
```

This should trigger a crash with full backtrace logged to NVS.

### Load Testing
1. Set download interval to 30 seconds
2. Enable multi-image cycling with 5 images
3. Send MQTT commands rapidly
4. Monitor heap/PSRAM over time

## Getting Help

When reporting crashes, include:
1. **Firmware version** (from Console or Serial)
2. **Complete crash log** (download from Console page)
3. **System info** (heap, PSRAM, uptime before crash)
4. **Backtrace addresses** (all PC values)
5. **Steps to reproduce** (what you were doing)

Post issues at: https://github.com/chvvkumar/ESP32-P4-Allsky-Display/issues
