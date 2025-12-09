# ESP32-P4 AllSky Display - Troubleshooting Guide

## Table of Contents
- [Common Issues](#common-issues)
- [Crash Diagnosis](#crash-diagnosis)
- [Display Problems](#display-problems)
- [Network Issues](#network-issues)
- [Image Loading Problems](#image-loading-problems)
- [Memory Issues](#memory-issues)
- [Touch Problems](#touch-problems)
- [MQTT/Home Assistant Issues](#mqtthome-assistant-issues)
- [Performance Issues](#performance-issues)
- [Diagnostic Tools](#diagnostic-tools)

---

## Common Issues

### Issue: "Red Screen of Death" on Boot

**Symptoms:**
- Display shows solid red color
- Device appears frozen
- Serial monitor shows crash messages or repeated reboots

**Causes:**
- **PSRAM allocation failure** (most common)
- **Watchdog timeout** during initialization
- **Display init failure**
- **Crash during startup**

**Solutions:**

1. **Check PSRAM availability:**
   ```cpp
   // Add to setup() before buffer allocation
   Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
   Serial.printf("Free PSRAM before init: %d bytes\n", ESP.getFreePsram());
   ```
   **Expected:** ~32MB total, >25MB free before allocation

2. **Verify buffer allocation order:**
   - Buffers MUST be allocated BEFORE `displayManager.begin()`
   - Check `setup()` for correct sequence:
     ```cpp
     imageBuffer = ps_malloc(...);           // ✓ First
     fullImageBuffer = ps_malloc(...);       // ✓ Second
     pendingFullImageBuffer = ps_malloc(...);// ✓ Third
     scaledBuffer = ps_malloc(...);          // ✓ Fourth
     displayManager.begin();                 // ✓ Fifth (AFTER buffers)
     ```

3. **Reduce buffer sizes (temporary test):**
   ```cpp
   // In config.h
   #define SCALED_BUFFER_MULTIPLIER 2  // Reduce from 4 to 2
   // This limits max scale to 1.41× but uses less memory
   ```

4. **Check for memory fragmentation:**
   ```cpp
   Serial.printf("Largest free block: %d bytes\n",
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
   ```
   **Expected:** >5MB largest block available

5. **Factory reset NVS:**
   - Corrupted NVS can cause boot issues
   - Tools → ESP32 Sketch Data Upload → Erase Flash
   - Or serial command: `F` (factory reset)

**Prevention:**
- Always use `compile-and-upload.ps1` script (handles buffer ordering)
- Monitor PSRAM usage via Web Console after changes
- Test configuration changes before deploying to remote devices

---

### Issue: "No WiFi Connection" / Stuck in Captive Portal

**Symptoms:**
- Device creates `AllSky-Display-Setup` AP but won't connect to WiFi
- Serial shows "WiFi connection failed" messages
- QR code displays but credentials don't work

**Causes:**
- **Incorrect WiFi credentials**
- **MAC address not initialized** (00:00:00:00:00:00)
- **WiFi hardware init delay missing**
- **Router blocking new devices**
- **5GHz network selected** (ESP32 only supports 2.4GHz)

**Solutions:**

1. **Verify WiFi credentials:**
   - Check SSID is exactly correct (case-sensitive)
   - Verify password (no spaces at beginning/end)
   - Test credentials with another device first

2. **Check MAC address initialization:**
   ```cpp
   // Add to setup() after networkManager.begin()
   delay(500);  // CRITICAL: Allow MAC address init
   Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
   ```
   **Expected:** Non-zero MAC like `AA:BB:CC:DD:EE:FF`  
   **Problem:** If shows `00:00:00:00:00:00`, increase delay to 1000ms

3. **Confirm 2.4GHz network:**
   - ESP32 does NOT support 5GHz WiFi
   - Use 2.4GHz network only
   - Check router has 2.4GHz enabled (some routers disable it)

4. **Check router logs:**
   - Look for DHCP requests from ESP32's MAC
   - Verify no MAC filtering or whitelist blocking device
   - Check for "out of DHCP leases" errors

5. **Serial hardcoded WiFi (temporary test):**
   ```cpp
   // In config.cpp - for testing only
   const char* WIFI_SSID = "YourNetwork";
   const char* WIFI_PASSWORD = "YourPassword";
   ```
   Recompile and upload to bypass captive portal.

6. **Factory reset and retry:**
   - Serial command: `F`
   - Or erase flash via Arduino IDE
   - Restart captive portal process

**Prevention:**
- Use captive portal QR code for mobile devices (avoids typos)
- Document working credentials before deployment
- Test WiFi connection from device's physical location (signal strength)

---

### Issue: Device Reboots Every 30 Seconds (Watchdog Timeout)

**Symptoms:**
- Device boots, initializes, then reboots after ~30 seconds
- Serial shows "Task watchdog got triggered" messages
- Display flickers or resets

**Causes:**
- **Long-running operation without watchdog reset**
- **HTTP download taking >30 seconds**
- **Main loop blocked** (network issue, I2C hang, etc.)

**Solutions:**

1. **Increase watchdog timeout:**
   ```cpp
   // In config.h
   #define WATCHDOG_TIMEOUT_MS 60000  // Increase from 30s to 60s
   ```
   Or via Web UI: Advanced → Watchdog Timeout

2. **Check for blocking operations:**
   - Review recent code changes
   - Look for `while()` loops without `systemMonitor.forceResetWatchdog()`
   - Check for synchronous network calls

3. **Add watchdog resets in long functions:**
   ```cpp
   void myLongFunction() {
       WATCHDOG_SCOPE();  // RAII pattern (recommended)
       
       for (int i = 0; i < 1000; i++) {
           doExpensiveWork();
           if (i % 100 == 0) {
               systemMonitor.forceResetWatchdog();
           }
       }
   }
   ```

4. **Check network connectivity:**
   - Slow downloads can exceed watchdog timeout
   - Verify image URLs are accessible and respond quickly
   - Test download speed: `curl -w "%{time_total}\n" -o /dev/null <URL>`

5. **Monitor loop execution time:**
   ```cpp
   // In loop()
   unsigned long loopStart = millis();
   // ... loop body ...
   unsigned long loopTime = millis() - loopStart;
   if (loopTime > 1000) {
       Serial.printf("WARNING: Loop took %lu ms\n", loopTime);
   }
   ```
   **Expected:** Loop time <500ms typically

**Prevention:**
- Always use `systemMonitor.forceResetWatchdog()` in long operations
- Test with slow network conditions (mobile hotspot)
- Use `WATCHDOG_SCOPE()` macro for automatic resets in functions
- Monitor Web Console for "WARNING: Loop took" messages

---

## Crash Diagnosis

### Understanding PANIC/EXCEPTION Crashes

**What is a PANIC/EXCEPTION?**
The ESP32 encountered an **unhandled exception** and cannot continue:
- **LoadProhibited:** Tried to read from invalid memory address
- **StoreProhibited:** Tried to write to invalid memory address
- **IllegalInstruction:** Attempted to execute invalid CPU instruction
- **StackOverflow:** Function call stack exceeded available space
- **Divide by Zero:** Arithmetic exception

**Enhanced Crash Information (Post-Update):**

With NTP timestamps, crash logs now show:
```
[2025-12-08 13:27:03] [CrashLogger] !!! CRASH DETECTED !!! Reset reason: PANIC/EXCEPTION
[2025-12-08 13:27:03] [CrashLogger] CPU Freq: 360 MHz, XTAL: 40 MHz
[2025-12-08 13:27:03] [CrashLogger] Heap at crash: 317804 bytes free, Largest block: 118784 bytes
[2025-12-08 13:27:03] [CrashLogger] PSRAM at crash: 25165824 bytes free of 33554432 total
[2025-12-08 13:27:03] [CrashLogger] Backtrace (if available):
  [0] PC: 0x4200ABCD SP: 0x3FCE8000
  [1] PC: 0x4200DEF0 SP: 0x3FCE8020
  [2] PC: 0x42011234 SP: 0x3FCE8040
```

### Decoding Backtraces with addr2line

**Method 1: PowerShell Script**

```powershell
# Find ESP32 toolchain path
$TOOLCHAIN = "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\xtensa-esp32-elf-gcc\*\bin"

# Get .elf file from build
$ELF_FILE = ".\build\ESP32-P4-Allsky-Display.ino.elf"

# Decode crash address
& "$TOOLCHAIN\xtensa-esp32-elf-addr2line.exe" -e $ELF_FILE -f -p 0x4200ABCD
```

**Output Example:**
```
downloadImage() at C:/Users/Kumar/git/ESP32-P4-Allsky-Display/ESP32-P4-Allsky-Display.ino:1234
```

**Method 2: Arduino IDE Exception Decoder**

1. Install **ESP Exception Decoder** from Library Manager
2. Tools → ESP Exception Decoder
3. Paste entire crash dump from Serial Monitor
4. Decoder automatically shows function names and line numbers

**Method 3: Web Console**

1. Open `http://allskyesp32.lan:8080/console`
2. Look for "Crash Logs" section
3. Backtrace addresses shown with timestamps
4. Copy addresses and decode with addr2line

### Common Crash Patterns

#### Pattern 1: Crash During Image Download

**Symptoms:**
```
[Image] Downloading image data...
PANIC/EXCEPTION (LoadProhibited)
Backtrace: 0x4200xxxx ...
```

**Likely Causes:**
- Network buffer allocation failure during HTTP stream
- JPEG decoder buffer overflow (image too large)
- NULL pointer dereference (stream closed unexpectedly)

**Solutions:**
1. **Check image size:**
   ```
   curl -I https://your-image-url.com/image.jpg
   # Look for Content-Length header
   # Max supported: 4MB (1448×1448 pixels)
   ```

2. **Verify image format:**
   ```bash
   curl https://your-image-url.com/image.jpg | file -
   # Should show: "JPEG image data..."
   # If PNG: "PNG image data..." - NOT SUPPORTED
   ```

3. **Test download speed:**
   ```bash
   time curl https://your-image-url.com/image.jpg -o test.jpg
   # Should complete in <30 seconds (watchdog timeout)
   ```

4. **Monitor free memory before download:**
   ```cpp
   // In downloadAndDisplayImage()
   LOG_INFO_F("Free heap before download: %d bytes", systemMonitor.getCurrentFreeHeap());
   LOG_INFO_F("Free PSRAM before download: %d bytes", systemMonitor.getCurrentFreePsram());
   ```

---

#### Pattern 2: Crash During Display Update (PPA-related)

**Symptoms:**
```
[PPA] Attempting hardware scale+rotate...
PANIC/EXCEPTION (StoreProhibited)
Backtrace: 0x42xxxx ...
```

**Likely Causes:**
- DMA buffer misalignment (PPA requires 64-byte alignment)
- Destination buffer too small for scaled image
- Cache coherency issue (data not flushed before PPA access)

**Solutions:**
1. **Verify buffer allocation uses aligned allocator:**
   ```cpp
   // In ppa_accelerator.cpp - check this is used:
   ppa_src_buffer = (uint16_t*)heap_caps_aligned_alloc(
       64,  // ← MUST be 64-byte aligned
       FULL_IMAGE_BUFFER_SIZE,
       MALLOC_CAP_SPIRAM
   );
   ```

2. **Check scaled buffer size:**
   ```cpp
   size_t scaledImageSize = dstWidth * dstHeight * 2;
   if (scaledImageSize > scaledBufferSize) {
       Serial.println("ERROR: Scaled image exceeds buffer!");
       // Fall back to software rendering
   }
   ```

3. **Increase SCALED_BUFFER_MULTIPLIER if overshooting:**
   ```cpp
   // In config.h
   #define SCALED_BUFFER_MULTIPLIER 9  // Increase from 4 to 9 (max 3.0× scale)
   ```

4. **Disable PPA temporarily (test):**
   ```cpp
   // In renderFullImage()
   if (false && ppaAccelerator.isAvailable()) {  // Force software fallback
       // ...
   }
   ```

---

#### Pattern 3: Crash on Boot / WiFi Setup

**Symptoms:**
```
Initializing WiFi...
PANIC/EXCEPTION (IllegalInstruction)
Boot loop: Device reboots repeatedly
```

**Likely Causes:**
- NVS corruption (stored configuration invalid)
- Captive portal DNS server crash
- WiFi hardware not initialized properly

**Solutions:**
1. **Factory reset NVS:**
   - Serial command: `F`
   - Or Arduino IDE: Tools → Erase Flash → All Flash Contents

2. **Check NVS namespace:**
   ```cpp
   // Add debug output in configStorage.begin()
   Serial.println("Opening NVS namespace 'config'...");
   bool success = preferences.begin("config", false);
   Serial.printf("NVS open: %s\n", success ? "OK" : "FAILED");
   ```

3. **Bypass captive portal (test):**
   ```cpp
   // In setup() - temporary test
   configStorage.setWiFiSSID("TestNetwork");
   configStorage.setWiFiPassword("TestPassword");
   configStorage.setWiFiProvisioned(true);
   configStorage.saveConfig();
   ```

4. **Check for boot loop:**
   ```cpp
   // In setup()
   if (crashLogger.getBootCount() > 10) {
       Serial.println("WARNING: Multiple rapid reboots detected!");
       // Force safe mode or factory reset
   }
   ```

---

#### Pattern 4: Crash During MQTT Operations

**Symptoms:**
```
MQTT: Processing command...
PANIC/EXCEPTION (LoadProhibited)
```

**Likely Causes:**
- MQTT payload buffer overflow (message >2KB)
- JSON parsing error (malformed command from HA)
- String allocation failure (heap fragmentation)

**Solutions:**
1. **Check MQTT buffer size:**
   ```cpp
   // In PubSubClient library
   #define MQTT_MAX_PACKET_SIZE 2048  // Should be ≥2048
   ```

2. **Validate JSON before parsing:**
   ```cpp
   // In handleCommand()
   if (payload.length() > MQTT_MAX_PACKET_SIZE) {
       LOG_ERROR("MQTT payload too large!");
       return;
   }
   ```

3. **Monitor heap before MQTT operations:**
   ```cpp
   // In mqttManager.loop()
   size_t heapBefore = ESP.getFreeHeap();
   mqttClient.loop();
   size_t heapAfter = ESP.getFreeHeap();
   if (heapBefore - heapAfter > 10000) {
       LOG_WARNING_F("MQTT consumed %d bytes heap", heapBefore - heapAfter);
   }
   ```

4. **Test HA commands individually:**
   ```bash
   # Test brightness command
   mosquitto_pub -t "homeassistant/allsky_display/light/command" \
     -m '{"brightness":50}'
   
   # Monitor device response
   mosquitto_sub -t "homeassistant/allsky_display/#" -v
   ```

---

## Display Problems

### Issue: Blank/Black Screen (Display Doesn't Turn On)

**Symptoms:**
- Display remains black after power-on
- Backlight on (screen glowing) but no image
- Serial shows successful init messages

**Causes:**
- **Incorrect vendor init sequence** (wrong display selected)
- **Display timing mismatch**
- **DSI lane configuration error**
- **Backlight PWM not configured**

**Solutions:**
1. **Verify CURRENT_SCREEN matches hardware:**
   ```cpp
   // In displays_config.h
   #define CURRENT_SCREEN SCREEN_3INCH_4_DSI  // or SCREEN_4INCH_DSI
   ```

2. **Check display initialization:**
   ```cpp
   // Add debug in displayManager.begin()
   Serial.println("Initializing DSI panel...");
   if (!dsipanel) {
       Serial.println("ERROR: DSI panel creation failed!");
   }
   if (!gfx) {
       Serial.println("ERROR: GFX init failed!");
   }
   ```

3. **Test backlight control:**
   ```cpp
   // In setup() after display init
   displayManager.setBrightness(100);  // Force maximum brightness
   delay(1000);
   displayManager.clearScreen(COLOR_WHITE);  // Show white screen
   ```

4. **Verify vendor init sequence:**
   - Compare `vendor_specific_init_default[]` with display datasheet
   - Check for missing "Sleep Out" (0x11) or "Display On" (0x29) commands

5. **Try alternate display config:**
   - Temporarily switch `CURRENT_SCREEN` to other display type
   - If works, your display may be mislabeled or different vendor

---

### Issue: Display Shows Corrupted/Scrambled Colors

**Symptoms:**
- Image appears but colors are wrong (green/purple tint)
- Horizontal or vertical lines visible
- Image fragmented or shifted

**Causes:**
- **Wrong color format** (RGB888 instead of RGB565)
- **Display timing parameters incorrect**
- **DSI clock mismatch**
- **Memory corruption**

**Solutions:**
1. **Verify RGB565 format:**
   ```cpp
   // In JPEG decode callback
   Serial.printf("Pixel format: RGB565 (16-bit)\n");
   Serial.printf("Sample pixel: 0x%04X\n", pDraw->pPixels[0]);
   ```

2. **Check display timing:**
   ```cpp
   // In displays_config.h
   .prefer_speed = 80000000,  // 80MHz pixel clock
   .lane_bit_rate = 1000000000,  // 1Gbps per lane
   ```
   Try reducing `lane_bit_rate` to 800000000 (800Mbps) if corrupted.

3. **Test with solid colors:**
   ```cpp
   // In loop() - temporary test
   displayManager.clearScreen(COLOR_RED);
   delay(1000);
   displayManager.clearScreen(COLOR_GREEN);
   delay(1000);
   displayManager.clearScreen(COLOR_BLUE);
   ```
   If solid colors work but images don't, problem is in image decoding.

4. **Check for memory corruption:**
   ```cpp
   // Before rendering
   Serial.printf("First pixel: 0x%04X\n", fullImageBuffer[0]);
   Serial.printf("Last pixel: 0x%04X\n", fullImageBuffer[fullImageWidth * fullImageHeight - 1]);
   ```

---

### Issue: Display Flickers During Image Updates

**Symptoms:**
- Black flash between images
- Screen briefly goes dark before new image appears
- "Tearing" effect during updates

**Causes:**
- **Clearing screen between updates** (flicker fix disabled)
- **Single buffering** (pendingFullImageBuffer not used)
- **Display not paused during heavy operations**

**Solutions:**
1. **Verify double-buffering is active:**
   ```cpp
   // In downloadAndDisplayImage()
   // Decode should write to pendingFullImageBuffer
   if (jpeg.openRAM(..., JPEGDraw)) {
       // JPEGDraw callback writes to pendingFullImageBuffer
   }
   
   // In loop()
   if (imageReadyToDisplay) {
       // Swap buffers
       uint16_t* temp = fullImageBuffer;
       fullImageBuffer = pendingFullImageBuffer;
       pendingFullImageBuffer = temp;
       imageReadyToDisplay = false;
       renderFullImage();  // Display swapped buffer
   }
   ```

2. **Disable screen clearing (after first image):**
   ```cpp
   // In renderFullImage()
   if (!hasSeenFirstImage) {
       displayManager.clearScreen();  // Only on first image
       hasSeenFirstImage = true;
   }
   // No clearing on subsequent images - prevents flicker!
   ```

3. **Use pauseDisplay() during heavy ops:**
   ```cpp
   displayManager.pauseDisplay();
   jpeg.decode(0, 0, 0);  // Heavy JPEG decode
   displayManager.resumeDisplay();
   ```

---

## Network Issues

### Issue: Slow Image Downloads (>30 seconds)

**Symptoms:**
- Watchdog timeout during download
- "Download stalled" errors in logs
- Images partially downloaded

**Causes:**
- **Slow WiFi connection** (weak signal, interference)
- **Server response slow**
- **Large image files** (>2MB)
- **Network congestion**

**Solutions:**
1. **Check WiFi signal strength:**
   ```cpp
   int rssi = WiFi.RSSI();
   Serial.printf("WiFi signal: %d dBm ", rssi);
   if (rssi < -75) Serial.println("(WEAK)");
   else if (rssi < -65) Serial.println("(Fair)");
   else Serial.println("(Good)");
   ```
   **Target:** >-70 dBm for reliable operation

2. **Test download speed from PC:**
   ```bash
   time curl https://your-image-url.com/image.jpg -o test.jpg
   # Should complete in <15 seconds
   ```

3. **Reduce image size at source:**
   - AllSky cameras often have configurable output sizes
   - Try 512×512 instead of 1024×1024
   - Use JPEG quality 80 instead of 95 (smaller files)

4. **Increase timeout temporarily:**
   ```cpp
   // In config.h
   #define TOTAL_DOWNLOAD_TIMEOUT 180000  // 3 minutes instead of 90s
   ```

5. **Monitor download progress:**
   - Check Web Console during download
   - Look for "Progress: X%" messages
   - Identify where download stalls

---

### Issue: DNS Resolution Failures

**Symptoms:**
- "HTTP begin failed" errors
- "Connection refused" despite correct URL
- Works with IP address but not hostname

**Causes:**
- **DNS server not responding**
- **Firewall blocking DNS (port 53)**
- **Router DNS cache stale**

**Solutions:**
1. **Test DNS resolution:**
   ```cpp
   // Add before http.begin()
   IPAddress resolved;
   if (WiFi.hostByName("example.com", resolved)) {
       Serial.printf("Resolved to: %s\n", resolved.toString().c_str());
   } else {
       Serial.println("DNS resolution failed!");
   }
   ```

2. **Use IP address temporarily:**
   ```
   Instead of: https://camera.example.com/image.jpg
   Use: https://192.168.1.50/image.jpg
   ```

3. **Set custom DNS servers:**
   ```cpp
   // In networkManager.cpp after WiFi.begin()
   WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE,
               IPAddress(8,8,8,8),    // Google DNS
               IPAddress(1,1,1,1));   // Cloudflare DNS
   ```

4. **Check router DNS settings:**
   - Verify router has valid upstream DNS
   - Restart router to clear DNS cache
   - Try different DNS servers (Google, Cloudflare, ISP)

---

## Image Loading Problems

### Issue: "PNG format detected - not supported"

**Symptoms:**
- Error message: "PNG format detected (0x89504E47)"
- Image won't load despite valid URL
- Works in browser but not on device

**Cause:**
- JPEGDEC library **only supports JPEG** format
- PNG, WebP, GIF, etc. are not supported

**Solutions:**
1. **Convert image to JPEG:**
   ```bash
   # Using ImageMagick
   convert input.png output.jpg
   
   # Or with Python PIL
   from PIL import Image
   img = Image.open("input.png")
   img.convert("RGB").save("output.jpg", "JPEG")
   ```

2. **Configure AllSky camera for JPEG:**
   - Most AllSky cameras output JPEG by default
   - Check camera settings for format selection
   - Ensure no image processing pipeline converts to PNG

3. **Use image proxy/converter:**
   - Create web service that converts PNG → JPEG on-the-fly
   - Example: `https://your-server.com/convert?url=...`

---

### Issue: "Image too large for buffer"

**Symptoms:**
- Error: "Image exceeds buffer capacity!"
- Message shows required vs. available bytes
- Image >1448×1448 pixels

**Causes:**
- Source image dimensions exceed `FULL_IMAGE_BUFFER_SIZE` (4MB)
- Max supported: sqrt(4MB / 2 bytes) = 1448 pixels per side

**Solutions:**
1. **Resize image at source:**
   - Configure AllSky camera for smaller output (800×800 or 1024×1024)
   - Most cameras default to 1024×1024 (works fine)

2. **Use image resizing proxy:**
   - Create web service that resizes images before download
   - Example: `https://your-server.com/resize/800/image.jpg`

3. **Increase buffer size (requires re-flash):**
   ```cpp
   // In config.h
   #define FULL_IMAGE_BUFFER_SIZE (8 * 1024 * 1024)  // 8MB (max ~2048×2048)
   ```
   **Warning:** Uses more PSRAM, may reduce available memory for other operations.

---

## Memory Issues

### Issue: "Out of memory" / Allocation Failures

**Symptoms:**
- "Failed to allocate buffer" errors
- `ps_malloc()` returns NULL
- Device reboots with "Heap/PSRAM low" warnings

**Causes:**
- **PSRAM fragmentation** (many small allocations/frees)
- **Memory leak** (allocations without matching frees)
- **Buffer sizes exceed available PSRAM**

**Solutions:**
1. **Check current memory status:**
   ```cpp
   Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
   Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
   Serial.printf("Largest PSRAM block: %d bytes\n",
                 heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
   ```
   **Healthy:** >100KB heap, >10MB PSRAM, >5MB largest block

2. **Monitor for memory leaks:**
   ```cpp
   // In loop() - log minimum values
   static size_t minHeap = 999999;
   size_t currentHeap = ESP.getFreeHeap();
   if (currentHeap < minHeap) {
       minHeap = currentHeap;
       Serial.printf("New heap minimum: %d bytes\n", minHeap);
   }
   ```
   **Problem:** If minimum keeps decreasing over time (leak present)

3. **Reduce buffer sizes temporarily:**
   ```cpp
   #define SCALED_BUFFER_MULTIPLIER 2  // From 4 → limits max scale to 1.41×
   ```

4. **Check for double-allocations:**
   ```cpp
   // Ensure buffers only allocated once
   if (fullImageBuffer == nullptr) {
       fullImageBuffer = (uint16_t*)ps_malloc(FULL_IMAGE_BUFFER_SIZE);
   } else {
       Serial.println("WARNING: Buffer already allocated!");
   }
   ```

5. **Use Web API to monitor runtime:**
   ```bash
   curl http://allskyesp32.lan:8080/api/info | jq '.memory'
   ```

---

### Issue: Memory Warnings but System Still Works

**Symptoms:**
- "WARNING: Low heap" messages in logs
- Memory below threshold but no crashes
- Device continues operating normally

**Causes:**
- **Thresholds set too high** (overly sensitive)
- **Normal fluctuation** during image processing
- **Heap used by libraries** (WebServer, MQTT, etc.)

**Solutions:**
1. **Adjust memory thresholds:**
   ```
   Web UI → Advanced → Critical Heap Threshold
   - Default: 50KB
   - Increase to 30KB if warnings excessive
   ```

2. **Monitor memory patterns:**
   - Check Web Console during image download
   - Note minimum heap during operation
   - Set threshold below normal minimum

3. **Understand memory usage:**
   ```
   Heap: ~200-300KB (WiFi stack, MQTT, WebServer)
   PSRAM: ~15.68MB allocated buffers
          ~16.32MB free for operations
   ```
   Warnings are informational - system can handle low heap.

---

## Touch Problems

### Issue: Touch Not Responding

**Symptoms:**
- Screen doesn't respond to taps
- Touch controller not detected at boot
- I2C errors in serial log

**Causes:**
- **GT911 not connected** or faulty cable
- **Wrong I2C pins** (3.4" vs 4.0" display)
- **I2C bus not initialized**
- **Touch controller in reset**

**Solutions:**
1. **Verify I2C pin configuration:**
   ```cpp
   // Check displays_config.h for your display
   // 3.4" display:
   .i2c_sda_pin = 8,
   .i2c_scl_pin = 18,
   
   // 4.0" display:
   .i2c_sda_pin = 8,
   .i2c_scl_pin = 9,
   ```

2. **Test I2C scan:**
   ```cpp
   // Add to setup()
   Wire.begin(8, 18);  // SDA, SCL
   for (uint8_t addr = 1; addr < 127; addr++) {
       Wire.beginTransmission(addr);
       if (Wire.endTransmission() == 0) {
           Serial.printf("I2C device found at 0x%02X\n", addr);
       }
   }
   ```
   **Expected:** GT911 at 0x5D or 0x14

3. **Check touch cable connection:**
   - Reseat 6-pin touch connector
   - Verify cable not damaged
   - Try different touch controller if available

4. **Disable touch temporarily (test):**
   ```cpp
   // In initializeTouchController()
   return;  // Skip touch init to test rest of system
   ```

---

### Issue: Touch Works but Gestures Not Recognized

**Symptoms:**
- Touch detected but single/double tap not working
- Touch state visible in logs but no action taken

**Causes:**
- **Timing parameters too strict** (double-tap window)
- **Touch coordinates inverted** or incorrect
- **Gesture processing disabled**

**Solutions:**
1. **Adjust gesture timing:**
   ```cpp
   // In config.h
   #define DOUBLE_TAP_TIMEOUT_MS 600  // Increase from 400ms
   #define MIN_TAP_DURATION_MS 30     // Decrease from 50ms
   ```

2. **Debug touch events:**
   ```cpp
   // In updateTouchState()
   Serial.printf("Touch: X=%d Y=%d Pressed=%d\n", x, y, pressed);
   ```

3. **Test single tap first:**
   - Try single taps only (advance image)
   - Verify action occurs before testing double-tap

4. **Check touch mode:**
   ```cpp
   Serial.printf("Cycling mode: %s\n", cyclingEnabled ? "ON" : "OFF");
   Serial.printf("Single image refresh mode: %s\n",
                 singleImageRefreshMode ? "ON" : "OFF");
   ```

---

## MQTT/Home Assistant Issues

### Issue: Device Not Appearing in Home Assistant

**Symptoms:**
- No entities created in HA
- MQTT broker shows no messages from device
- Device logs show "MQTT connected" but no activity

**Causes:**
- **HA MQTT integration not configured**
- **MQTT discovery disabled** in device
- **Wrong discovery prefix** (default vs. custom)
- **Firewall blocking MQTT port 1883**

**Solutions:**
1. **Verify MQTT broker config in HA:**
   ```yaml
   # configuration.yaml
   mqtt:
     broker: localhost
     port: 1883
     discovery: true  # ← MUST be enabled
   ```

2. **Check device MQTT settings:**
   ```
   Web UI → MQTT Configuration
   - Discovery Enabled: ON
   - Discovery Prefix: homeassistant
   - Server: <HA IP or hostname>
   ```

3. **Monitor MQTT discovery messages:**
   ```bash
   # Subscribe to discovery topics
   mosquitto_sub -h homeassistant.local -t "homeassistant/#" -v | grep allsky
   ```
   **Expected:** Many `homeassistant/light/allsky_display_*/config` messages

4. **Force discovery republish:**
   - Restart device (triggers discovery on boot)
   - Or send MQTT command: 
     ```bash
     mosquitto_pub -t "homeassistant/allsky_display/command" \
       -m '{"action":"republish_discovery"}'
     ```

5. **Check HA logs:**
   ```
   Settings → System → Logs
   Filter for "mqtt" or "allsky"
   Look for "Entity added" or "Failed to setup" messages
   ```

---

### Issue: MQTT Commands Not Working

**Symptoms:**
- HA entities exist but controls don't affect device
- Brightness slider moves but display stays same
- "Next Image" button does nothing

**Causes:**
- **Command topic incorrect**
- **Payload format wrong** (JSON vs. plain text)
- **Device not subscribed to command topics**
- **MQTT loop not called** (blocking operation)

**Solutions:**
1. **Verify command topic:**
   ```bash
   # Test brightness command directly
   mosquitto_pub -t "homeassistant/allsky_display/light/command" \
     -m '{"state":"ON","brightness":75}'
   ```

2. **Monitor device subscription:**
   ```bash
   # Check if device subscribed to command topics
   mosquitto_sub -t "homeassistant/allsky_display/+/command" -v
   ```
   Device should subscribe to all command topics on connect.

3. **Check MQTT loop in code:**
   ```cpp
   // In loop()
   mqttManager.update();  // Calls mqttClient.loop() internally
   ```

4. **Test with MQTT Explorer GUI:**
   - Download MQTT Explorer (http://mqtt-explorer.com/)
   - Connect to broker
   - Manually publish to command topics
   - Watch for responses on state topics

5. **Enable MQTT debug logging:**
   ```cpp
   // In mqtt_manager.cpp
   mqttClient.setDebug(true);  // Enable verbose logging
   ```

---

## Performance Issues

### Issue: Slow Image Rendering (>5 seconds)

**Symptoms:**
- Long delay between download complete and image appearing
- PPA acceleration not working
- Software fallback used

**Causes:**
- **PPA hardware unavailable** (init failed)
- **Destination buffer too small** (falls back to software)
- **Display not paused** during PPA (memory contention)

**Solutions:**
1. **Verify PPA is available:**
   ```cpp
   if (ppaAccelerator.isAvailable()) {
       Serial.println("PPA: Hardware acceleration active");
   } else {
       Serial.println("PPA: Software fallback (SLOW!)");
   }
   ```

2. **Check scaled buffer size:**
   ```cpp
   size_t scaledImageSize = scaledWidth * scaledHeight * 2;
   Serial.printf("Scaled size: %d bytes, Buffer: %d bytes\n",
                 scaledImageSize, scaledBufferSize);
   if (scaledImageSize > scaledBufferSize) {
       Serial.println("ERROR: Image too large for buffer!");
   }
   ```

3. **Increase SCALED_BUFFER_MULTIPLIER:**
   ```cpp
   #define SCALED_BUFFER_MULTIPLIER 9  // From 4 to 9 (max 3.0× scale)
   ```

4. **Use display pause:**
   ```cpp
   displayManager.pauseDisplay();  // Before heavy PPA ops
   ppaAccelerator.scaleRotateImage(...);
   displayManager.resumeDisplay();  // After completion
   ```

5. **Benchmark PPA vs. software:**
   ```cpp
   unsigned long start = millis();
   // ... scaling operation ...
   unsigned long elapsed = millis() - start;
   Serial.printf("Rendering took %lu ms\n", elapsed);
   ```
   **Target:** <300ms with PPA, <3000ms software fallback

---

## Diagnostic Tools

### Web-Based Diagnostics

**System Status API:**
```bash
# Get complete system state as JSON
curl http://allskyesp32.lan:8080/api/info | jq .

# Extract specific values
curl -s http://allskyesp32.lan:8080/api/info | jq '.memory.freePsram'
curl -s http://allskyesp32.lan:8080/api/info | jq '.network.rssi'
curl -s http://allskyesp32.lan:8080/api/info | jq '.display.brightness'
```

**Real-Time Console:**
```
URL: http://allskyesp32.lan:8080/console
- Filter by severity (DEBUG/INFO/WARNING/ERROR/CRITICAL)
- Download logs as text file
- View crash logs (preserved from previous boots)
```

### Serial Monitor Commands

**Available Commands:**
```
H - Help (show all commands)
M - Memory status (heap/PSRAM)
I - System information (uptime, version, config)
V - Version info (firmware, git commit, build date)
N - Network status (IP, signal, connection)
R - Restart device
F - Factory reset (clear all config)
D - Download image now
C - Clear crash logs
S - System status (all info combined)
```

**Example Session:**
```
> M
Free Heap: 234567 bytes
Free PSRAM: 16384000 bytes
Min Heap: 198765 bytes
Min PSRAM: 15680000 bytes

> I
System Information:
  Uptime: 1234567 ms
  Firmware: v1.2.3
  Git Commit: abc123def
  Build Date: Dec 09 2025 19:30:45
  
> N
Network Status:
  WiFi: Connected
  SSID: MyNetwork
  IP: 192.168.1.100
  Signal: -65 dBm (Good)
  MQTT: Connected
```

### Logging Macros

Use in code for consistent logging:
```cpp
LOG_DEBUG("Verbose debugging message");
LOG_INFO("General information");
LOG_WARNING("Non-critical warning");
LOG_ERROR("Error occurred but continuing");
LOG_CRITICAL("Critical failure!");

// With printf-style formatting
LOG_INFO_F("Image size: %dx%d", width, height);
LOG_ERROR_F("HTTP error code: %d", httpCode);
```

**Severity Filtering:**
- Set via Web UI: Console → Severity buttons
- Or NVS: `config.setMinLogSeverity(LOG_WARNING)`
- Only messages ≥ severity level are shown in WebSocket console

---

## Summary

This troubleshooting guide covers the most common issues encountered with the ESP32-P4 AllSky Display:

**Critical Issues:**
- Red Screen of Death → PSRAM allocation order
- Watchdog timeouts → Add forceResetWatchdog() calls
- Crashes → Decode backtrace with addr2line

**Display Issues:**
- Blank screen → Verify CURRENT_SCREEN matches hardware
- Color corruption → Check RGB565 format and timing
- Flicker → Use double-buffering and avoid screen clears

**Network Issues:**
- No WiFi → Check MAC init delay, 2.4GHz network
- Slow downloads → Verify signal strength, increase timeouts
- DNS failures → Use IP address or custom DNS servers

**Image Issues:**
- PNG not supported → Convert to JPEG at source
- Image too large → Resize to ≤1448×1448 or increase buffer
- Loading failures → Check URL, format, size

**Memory Issues:**
- Out of memory → Reduce buffer sizes, check for leaks
- Fragmentation → Allocate buffers early, minimize frees

**Performance Issues:**
- Slow rendering → Verify PPA available, increase scaled buffer
- High latency → Check network speed, WiFi signal

**Diagnostic Tools:**
- Web Console: Real-time logs with severity filtering
- Serial Commands: Memory stats, system info, manual actions
- API Endpoint: Programmatic access to all status data
- addr2line: Decode crash backtraces to source code lines

For additional help, see `ARCHITECTURE.md` for system design details and `API_REFERENCE.md` for function documentation.
