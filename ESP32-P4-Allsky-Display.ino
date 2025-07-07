#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "displays_config.h"

// ESP-IDF includes for PPA hardware acceleration and watchdog management
extern "C" {
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

// WiFi Configuration - Update these with your credentials
const char* ssid = "";
const char* password = "";

// MQTT Configuration - Update these with your MQTT broker details
const char* mqtt_server = "192.168.1.250";  // Replace with your MQTT broker IP
const int mqtt_port = 1883;
const char* mqtt_user = "";                 // Leave empty if no authentication
const char* mqtt_password = "";             // Leave empty if no authentication
const char* mqtt_client_id = "ESP32_Allsky_Display";
const char* reboot_topic = "Astro/AllSky/display/reboot"; // Topic to subscribe for reboot control

// Brightness control configuration
const char* brightness_topic = "Astro/AllSky/display/brightness";           // Topic to control brightness (0-100)
const char* brightness_status_topic = "Astro/AllSky/display/brightness/status"; // Topic for brightness status

// Image Configuration
const char* imageURL = "https://allsky.challa.co:1982/current/resized/image.jpg"; // Random image service
const unsigned long updateInterval = 60000; // 60 seconds in milliseconds

// Image change detection
String lastETag = "";
unsigned long lastModified = 0;
size_t lastImageSize = 0;
unsigned long lastSuccessfulCheck = 0;
const unsigned long forceCheckInterval = 300000; // Force check every 5 minutes regardless of cache headers

// Display variables
int16_t w, h;
unsigned long lastUpdate = 0;
bool wifiConnected = false;
uint8_t* imageBuffer = nullptr;
size_t imageBufferSize = 0;
bool firstImageLoaded = false; // Track if first image has been loaded

// Image transformation variables
float scaleX = 1.2;  // Horizontal scale factor
float scaleY = 1.2;  // Vertical scale factor
int16_t offsetX = 0; // Horizontal offset from center
int16_t offsetY = 0; // Vertical offset from center (changed from 40 to 0)
float rotationAngle = 0.0; // Current rotation in degrees (0, 90, 180, 270)
int16_t originalImageWidth = 0;
int16_t originalImageHeight = 0;
size_t currentImageSize = 0; // Store the actual size of the current image data

// Scaling buffer for interpolated pixels
uint16_t* scaledBuffer = nullptr;
size_t scaledBufferSize = 0;

// Control constants
const float SCALE_STEP = 0.1;    // Scale increment/decrement
const int16_t MOVE_STEP = 10;    // Movement step in pixels
const float MIN_SCALE = 0.1;     // Minimum scale factor
const float MAX_SCALE = 3.0;     // Maximum scale factor
const float ROTATION_STEP = 90.0; // Rotation increment in degrees

// Debug display variables
int debugY = 50;
const int debugLineHeight = 30;
const int maxDebugLines = 20; // Maximum lines to show on screen (reduced due to larger font)
int debugLineCount = 0;
const int debugTextSize = 3; // Increased from 2 to 3 for better readability

// JPEG decoder
JPEGDEC jpeg;

#ifndef CURRENT_SCREEN
#define CURRENT_SCREEN SCREEN_3INCH_4_DSI
#endif

// Display objects - will be initialized in setup()
Arduino_ESP32DSIPanel *dsipanel = nullptr;
Arduino_DSI_Display *gfx = nullptr;

// MQTT objects
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000; // 5 seconds

// OTA Web Server
WebServer server(80);
unsigned long ota_progress_millis = 0;

// System health and watchdog management
unsigned long lastWatchdogReset = 0;
const unsigned long watchdogResetInterval = 1000; // Reset watchdog every 1 second
unsigned long lastMemoryCheck = 0;
const unsigned long memoryCheckInterval = 30000; // Check memory every 30 seconds
size_t minFreeHeap = SIZE_MAX;
size_t minFreePsram = SIZE_MAX;
bool systemHealthy = true;
unsigned long lastSerialFlush = 0;
const unsigned long serialFlushInterval = 5000; // Flush serial every 5 seconds

// Brightness control variables
#define BACKLIGHT_PIN 26        // GPIO26 from schematic (LCD_BL_PWM)
#define BACKLIGHT_CHANNEL 0     // LEDC channel
#define BACKLIGHT_FREQ 5000     // 5kHz PWM frequency  
#define BACKLIGHT_RESOLUTION 10 // 10-bit resolution (0-1023)
int displayBrightness = 100;    // Default brightness 100%
bool brightnessInitialized = false;

// ElegantOTA callback functions
void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // Optionally pause image updates during OTA
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // Device will restart automatically after successful OTA
}

// Function to print debug messages to screen
void debugPrint(const char* message, uint16_t color = WHITE) {
  if (firstImageLoaded) return; // Don't show debug after first image loads
  
  Serial.println(message); // Also print to serial
  
  gfx->setTextSize(debugTextSize);
  gfx->setTextColor(color);
  
  // Calculate text width for centering
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  gfx->getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
  
  // Center the text horizontally
  int16_t centerX = (w - textWidth) / 2;
  if (centerX < 10) centerX = 10; // Minimum margin
  
  gfx->setCursor(centerX, debugY);
  gfx->println(message);
  
  debugY += debugLineHeight;
  debugLineCount++;
  
  // Scroll if we reach the bottom
  if (debugY > h - 50 || debugLineCount > maxDebugLines) {
    // Clear screen and reset
    gfx->fillScreen(BLACK);
    debugY = 50;
    debugLineCount = 0;
    
    // Center the scroll header
    gfx->setTextSize(debugTextSize);
    gfx->setTextColor(YELLOW);
    const char* scrollMsg = "=== DEBUG LOG ===";
    gfx->getTextBounds(scrollMsg, 0, 0, &x1, &y1, &textWidth, &textHeight);
    centerX = (w - textWidth) / 2;
    gfx->setCursor(centerX, debugY);
    gfx->println(scrollMsg);
    debugY += debugLineHeight * 2;
  }
}

void debugPrintf(uint16_t color, const char* format, ...) {
  if (firstImageLoaded) return;
  
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  debugPrint(buffer, color);
}

// Bilinear interpolation scaling function using legacy implementation
void scaleImageChunk(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                     uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
  // Use legacy version (will replace with optimized version later)
  scaleImageChunkLegacy(srcPixels, srcWidth, srcHeight, dstPixels, dstWidth, dstHeight);
}

// Simple strip-based scaling function (replacement for scaleImageStripOptimized)
void scaleImageStripLegacy(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                          uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight,
                          float scaleX, float scaleY, int16_t stripY, int16_t stripHeight) {
  // Calculate the source Y range for this strip
  float srcStartY = stripY / scaleY;
  float srcEndY = (stripY + stripHeight) / scaleY;
  
  // Process each row in the strip
  for (int16_t y = 0; y < stripHeight; y++) {
    float srcY = (stripY + y) / scaleY;
    int16_t srcY1 = (int16_t)srcY;
    int16_t srcY2 = min(srcY1 + 1, srcHeight - 1);
    float yWeight = srcY - srcY1;
    
    for (int16_t x = 0; x < dstWidth; x++) {
      float srcX = x / scaleX;
      int16_t srcX1 = (int16_t)srcX;
      int16_t srcX2 = min(srcX1 + 1, srcWidth - 1);
      float xWeight = srcX - srcX1;
      
      // Ensure source coordinates are within bounds
      if (srcY1 >= srcHeight) srcY1 = srcHeight - 1;
      if (srcY2 >= srcHeight) srcY2 = srcHeight - 1;
      if (srcX1 >= srcWidth) srcX1 = srcWidth - 1;
      if (srcX2 >= srcWidth) srcX2 = srcWidth - 1;
      
      // Get the four surrounding pixels
      uint16_t p1 = srcPixels[srcY1 * srcWidth + srcX1]; // Top-left
      uint16_t p2 = srcPixels[srcY1 * srcWidth + srcX2]; // Top-right
      uint16_t p3 = srcPixels[srcY2 * srcWidth + srcX1]; // Bottom-left
      uint16_t p4 = srcPixels[srcY2 * srcWidth + srcX2]; // Bottom-right
      
      // Extract RGB components
      uint8_t r1 = (p1 >> 11) & 0x1F, g1 = (p1 >> 5) & 0x3F, b1 = p1 & 0x1F;
      uint8_t r2 = (p2 >> 11) & 0x1F, g2 = (p2 >> 5) & 0x3F, b2 = p2 & 0x1F;
      uint8_t r3 = (p3 >> 11) & 0x1F, g3 = (p3 >> 5) & 0x3F, b3 = p3 & 0x1F;
      uint8_t r4 = (p4 >> 11) & 0x1F, g4 = (p4 >> 5) & 0x3F, b4 = p4 & 0x1F;
      
      // Bilinear interpolation
      float r = r1 * (1 - xWeight) * (1 - yWeight) + r2 * xWeight * (1 - yWeight) +
                r3 * (1 - xWeight) * yWeight + r4 * xWeight * yWeight;
      float g = g1 * (1 - xWeight) * (1 - yWeight) + g2 * xWeight * (1 - yWeight) +
                g3 * (1 - xWeight) * yWeight + g4 * xWeight * yWeight;
      float b = b1 * (1 - xWeight) * (1 - yWeight) + b2 * xWeight * (1 - yWeight) +
                b3 * (1 - xWeight) * yWeight + b4 * xWeight * yWeight;
      
      uint16_t finalPixel = ((uint16_t)(r + 0.5) << 11) |
                           ((uint16_t)(g + 0.5) << 5) |
                           (uint16_t)(b + 0.5);
      
      dstPixels[y * dstWidth + x] = finalPixel;
    }
  }
}

// Legacy float-based scaling function (for performance comparison if needed)
void scaleImageChunkLegacy(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                          uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
  float xRatio = (float)srcWidth / dstWidth;  // int->float conversion
  float yRatio = (float)srcHeight / dstHeight; // int->float conversion
  
  for (int16_t y = 0; y < dstHeight; y++) {
    for (int16_t x = 0; x < dstWidth; x++) {
      float srcX = x * xRatio;    // int->float conversion per pixel
      float srcY = y * yRatio;    // int->float conversion per pixel
      
      int16_t x1 = (int16_t)srcX; // float->int conversion per pixel
      int16_t y1 = (int16_t)srcY; // float->int conversion per pixel
      int16_t x2 = min(x1 + 1, srcWidth - 1);
      int16_t y2 = min(y1 + 1, srcHeight - 1);
      
      float xWeight = srcX - x1; // float math per pixel
      float yWeight = srcY - y1; // float math per pixel
      
      // Get the four surrounding pixels
      uint16_t p1 = srcPixels[y1 * srcWidth + x1]; // Top-left
      uint16_t p2 = srcPixels[y1 * srcWidth + x2]; // Top-right
      uint16_t p3 = srcPixels[y2 * srcWidth + x1]; // Bottom-left
      uint16_t p4 = srcPixels[y2 * srcWidth + x2]; // Bottom-right
      
      // Extract RGB components (assuming RGB565 format)
      uint8_t r1 = (p1 >> 11) & 0x1F;
      uint8_t g1 = (p1 >> 5) & 0x3F;
      uint8_t b1 = p1 & 0x1F;
      
      uint8_t r2 = (p2 >> 11) & 0x1F;
      uint8_t g2 = (p2 >> 5) & 0x3F;
      uint8_t b2 = p2 & 0x1F;
      
      uint8_t r3 = (p3 >> 11) & 0x1F;
      uint8_t g3 = (p3 >> 5) & 0x3F;
      uint8_t b3 = p3 & 0x1F;
      
      uint8_t r4 = (p4 >> 11) & 0x1F;
      uint8_t g4 = (p4 >> 5) & 0x3F;
      uint8_t b4 = p4 & 0x1F;
      
      // Bilinear interpolation with float operations per pixel
      float r = r1 * (1 - xWeight) * (1 - yWeight) +
                r2 * xWeight * (1 - yWeight) +
                r3 * (1 - xWeight) * yWeight +
                r4 * xWeight * yWeight;
      
      float g = g1 * (1 - xWeight) * (1 - yWeight) +
                g2 * xWeight * (1 - yWeight) +
                g3 * (1 - xWeight) * yWeight +
                g4 * xWeight * yWeight;
      
      float b = b1 * (1 - xWeight) * (1 - yWeight) +
                b2 * xWeight * (1 - yWeight) +
                b3 * (1 - xWeight) * yWeight +
                b4 * xWeight * yWeight;
      
      // More float->int conversions per pixel
      uint16_t finalPixel = ((uint16_t)(r + 0.5) << 11) |
                           ((uint16_t)(g + 0.5) << 5) |
                           (uint16_t)(b + 0.5);
      
      dstPixels[y * dstWidth + x] = finalPixel;
    }
  }
}

// Global buffer for full image decoding
uint16_t* fullImageBuffer = nullptr;
size_t fullImageBufferSize = 0;
int16_t fullImageWidth = 0;
int16_t fullImageHeight = 0;

// PPA Hardware Acceleration variables
ppa_client_handle_t ppa_scaling_handle = nullptr;
bool ppa_available = false;
uint16_t* ppa_src_buffer = nullptr;  // DMA-aligned source buffer
uint16_t* ppa_dst_buffer = nullptr;  // DMA-aligned destination buffer
size_t ppa_src_buffer_size = 0;
size_t ppa_dst_buffer_size = 0;

// JPEG callback function to collect pixels into full image buffer
int JPEGDraw(JPEGDRAW *pDraw) {
  // Store pixels in the full image buffer
  if (fullImageBuffer && pDraw->y + pDraw->iHeight <= fullImageHeight) {
    for (int16_t y = 0; y < pDraw->iHeight; y++) {
      uint16_t* destRow = fullImageBuffer + ((pDraw->y + y) * fullImageWidth + pDraw->x);
      uint16_t* srcRow = pDraw->pPixels + (y * pDraw->iWidth);
      memcpy(destRow, srcRow, pDraw->iWidth * sizeof(uint16_t));
    }
  }
  return 1;
}

// Initialize PPA hardware acceleration
bool initPPA() {
  debugPrint("Initializing PPA hardware...", YELLOW);
  
  // Configure PPA client for scaling operations
  ppa_client_config_t ppa_client_config = {
    .oper_type = PPA_OPERATION_SRM,  // Scaling, Rotating, and Mirror operations
  };
  
  esp_err_t ret = ppa_register_client(&ppa_client_config, &ppa_scaling_handle);
  if (ret != ESP_OK) {
    debugPrintf(RED, "PPA client registration failed: %s", esp_err_to_name(ret));
    return false;
  }
  
  // Allocate DMA-aligned buffers for PPA operations
  // Source buffer - for original image data
  ppa_src_buffer_size = 1024 * 1024; // 1MB for source (512x512 max at 16-bit)
  ppa_src_buffer = (uint16_t*)heap_caps_aligned_alloc(64, ppa_src_buffer_size, 
                                                      MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
  
  if (!ppa_src_buffer) {
    debugPrint("ERROR: PPA source buffer allocation failed!", RED);
    ppa_unregister_client(ppa_scaling_handle);
    return false;
  }
  
  // Destination buffer - for scaled image data (increased for scaling operations)
  ppa_dst_buffer_size = w * h * 2 * 2; // 2x display size to handle scaling up to 2x
  ppa_dst_buffer = (uint16_t*)heap_caps_aligned_alloc(64, ppa_dst_buffer_size, 
                                                      MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
  
  if (!ppa_dst_buffer) {
    debugPrint("ERROR: PPA destination buffer allocation failed!", RED);
    heap_caps_free(ppa_src_buffer);
    ppa_unregister_client(ppa_scaling_handle);
    return false;
  }
  
  debugPrint("PPA hardware initialized successfully!", GREEN);
  debugPrintf(WHITE, "PPA src buffer: %d bytes", ppa_src_buffer_size);
  debugPrintf(WHITE, "PPA dst buffer: %d bytes", ppa_dst_buffer_size);
  
  return true;
}

// Hardware-accelerated image scaling and rotation using PPA
bool scaleRotateImagePPA(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                         uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight, 
                         float rotation = 0.0) {
  if (!ppa_available || !ppa_scaling_handle) {
    Serial.println("DEBUG: PPA not available or handle invalid");
    return false; // Fall back to software scaling
  }
  
  // Convert rotation angle to PPA enum
  ppa_srm_rotation_angle_t ppa_rotation;
  if (rotation == 0.0) {
    ppa_rotation = PPA_SRM_ROTATION_ANGLE_0;
  } else if (rotation == 90.0) {
    ppa_rotation = PPA_SRM_ROTATION_ANGLE_90;
  } else if (rotation == 180.0) {
    ppa_rotation = PPA_SRM_ROTATION_ANGLE_180;
  } else if (rotation == 270.0) {
    ppa_rotation = PPA_SRM_ROTATION_ANGLE_270;
  } else {
    Serial.printf("DEBUG: Invalid rotation angle: %.1f\n", rotation);
    return false; // Invalid rotation angle
  }
  
  // Check if source fits in PPA buffer
  size_t srcSize = srcWidth * srcHeight * sizeof(uint16_t);
  if (srcSize > ppa_src_buffer_size) {
    Serial.printf("DEBUG: Source too large for PPA (%d > %d)\n", srcSize, ppa_src_buffer_size);
    return false; // Too large for hardware acceleration
  }
  
  // Check if destination fits in PPA buffer
  size_t dstSize = dstWidth * dstHeight * sizeof(uint16_t);
  if (dstSize > ppa_dst_buffer_size) {
    Serial.printf("DEBUG: Destination too large for PPA (%d > %d)\n", dstSize, ppa_dst_buffer_size);
    return false; // Too large for hardware acceleration
  }
  
  Serial.printf("DEBUG: PPA scale+rotate %dx%d -> %dx%d (%.1f°, src:%d dst:%d bytes)\n", 
               srcWidth, srcHeight, dstWidth, dstHeight, rotation, srcSize, dstSize);
  
  // Copy source data to DMA-aligned buffer
  memcpy(ppa_src_buffer, srcPixels, srcSize);
  
  // Ensure cache coherency for DMA operations
  esp_cache_msync(ppa_src_buffer, srcSize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  
  // Configure PPA scaling and rotation operation
  ppa_srm_oper_config_t srm_oper_config = {};
  
  // Source configuration
  srm_oper_config.in.buffer = ppa_src_buffer;
  srm_oper_config.in.pic_w = srcWidth;
  srm_oper_config.in.pic_h = srcHeight;
  srm_oper_config.in.block_w = srcWidth;
  srm_oper_config.in.block_h = srcHeight;
  srm_oper_config.in.block_offset_x = 0;
  srm_oper_config.in.block_offset_y = 0;
  srm_oper_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Output configuration
  srm_oper_config.out.buffer = ppa_dst_buffer;
  srm_oper_config.out.buffer_size = ppa_dst_buffer_size;
  srm_oper_config.out.pic_w = dstWidth;
  srm_oper_config.out.pic_h = dstHeight;
  srm_oper_config.out.block_offset_x = 0;
  srm_oper_config.out.block_offset_y = 0;
  srm_oper_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
  
  // Scaling configuration
  srm_oper_config.scale_x = (float)dstWidth / srcWidth;
  srm_oper_config.scale_y = (float)dstHeight / srcHeight;
  
  // Rotation configuration
  srm_oper_config.rotation_angle = ppa_rotation;
  
  // Mirror configuration (no mirroring)
  srm_oper_config.mirror_x = false;
  srm_oper_config.mirror_y = false;
  
  // Additional configuration
  srm_oper_config.rgb_swap = false;
  srm_oper_config.byte_swap = false;
  srm_oper_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
  srm_oper_config.mode = PPA_TRANS_MODE_BLOCKING;
  srm_oper_config.user_data = nullptr;
  
  Serial.printf("DEBUG: PPA scale factors: x=%.3f, y=%.3f, rotation=%d\n", 
               srm_oper_config.scale_x, srm_oper_config.scale_y, (int)ppa_rotation);
  
  // Perform the scaling and rotation operation
  esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_scaling_handle, &srm_oper_config);
  if (ret != ESP_OK) {
    Serial.printf("PPA scale+rotate operation failed: %s (0x%x)\n", esp_err_to_name(ret), ret);
    return false;
  }
  
  // Ensure cache coherency for reading results
  esp_cache_msync(ppa_dst_buffer, dstSize, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  
  // Copy result to destination buffer
  memcpy(dstPixels, ppa_dst_buffer, dstSize);
  
  Serial.println("DEBUG: PPA scale+rotate successful!");
  return true;
}

// Legacy function for backward compatibility
bool scaleImagePPA(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                   uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
  return scaleRotateImagePPA(srcPixels, srcWidth, srcHeight, dstPixels, dstWidth, dstHeight, 0.0);
}

// Cleanup PPA resources
void cleanupPPA() {
  if (ppa_scaling_handle) {
    ppa_unregister_client(ppa_scaling_handle);
    ppa_scaling_handle = nullptr;
  }
  
  if (ppa_src_buffer) {
    heap_caps_free(ppa_src_buffer);
    ppa_src_buffer = nullptr;
  }
  
  if (ppa_dst_buffer) {
    heap_caps_free(ppa_dst_buffer);
    ppa_dst_buffer = nullptr;
  }
  
  ppa_available = false;
}

// System health monitoring and watchdog management
void resetWatchdog() {
  unsigned long now = millis();
  if (now - lastWatchdogReset >= watchdogResetInterval) {
    esp_task_wdt_reset();
    lastWatchdogReset = now;
  }
}

// Force immediate watchdog reset (for critical sections)
void forceResetWatchdog() {
  esp_task_wdt_reset();
  lastWatchdogReset = millis();
}

void checkSystemHealth() {
  unsigned long now = millis();
  if (now - lastMemoryCheck >= memoryCheckInterval) {
    size_t freeHeap = ESP.getFreeHeap();
    size_t freePsram = ESP.getFreePsram();
    
    // Track minimum memory levels
    if (freeHeap < minFreeHeap) minFreeHeap = freeHeap;
    if (freePsram < minFreePsram) minFreePsram = freePsram;
    
    // Check for critical memory levels
    bool heapCritical = freeHeap < 50000;  // Less than 50KB heap
    bool psramCritical = freePsram < 100000; // Less than 100KB PSRAM
    
    if (heapCritical || psramCritical) {
      systemHealthy = false;
      Serial.printf("CRITICAL: Low memory - Heap: %d, PSRAM: %d\n", freeHeap, freePsram);
      
      // Force garbage collection
      if (heapCritical) {
        Serial.println("Attempting heap cleanup...");
        // Could add specific cleanup here if needed
      }
    } else {
      systemHealthy = true;
    }
    
    // Log memory status periodically
    Serial.printf("Memory status - Heap: %d (min: %d), PSRAM: %d (min: %d)\n", 
                 freeHeap, minFreeHeap, freePsram, minFreePsram);
    
    lastMemoryCheck = now;
  }
}

void flushSerial() {
  unsigned long now = millis();
  if (now - lastSerialFlush >= serialFlushInterval) {
    Serial.flush();
    lastSerialFlush = now;
  }
}

// Safe task yielding function
void safeYield() {
  resetWatchdog();
  yield();
  vTaskDelay(1); // Give other tasks a chance to run
}

// Safe delay with watchdog reset
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    resetWatchdog();
    delay(min(100UL, ms - (millis() - start)));
  }
}

// Brightness control functions
void initBrightness() {
  if (brightnessInitialized) return;
  
  debugPrint("Initializing brightness control...", YELLOW);
  
  // Configure LEDC for backlight control using newer API
  if (ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION) == 0) {
    debugPrint("ERROR: LEDC attach failed!", RED);
    return;
  }
  
  // Set initial brightness
  setBrightness(displayBrightness);
  brightnessInitialized = true;
  
  debugPrint("Brightness control initialized!", GREEN);
}

// Set brightness level (0-100%)
void setBrightness(int brightness) {
  if (!brightnessInitialized) return;
  
  brightness = constrain(brightness, 0, 100);
  displayBrightness = brightness;
  
  // Convert percentage to inverted 10-bit duty cycle (0-1023)
  // Backlight uses inverted logic: low PWM = high brightness
  uint32_t duty = 1023 - ((1023 * brightness) / 100);
  
  ledcWrite(BACKLIGHT_PIN, duty);
  
  Serial.printf("Display brightness set to: %d%% (PWM duty: %d)\n", brightness, duty);
}

// Get current brightness
int getBrightness() {
  return displayBrightness;
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("MQTT message received on topic '%s': %s\n", topic, message.c_str());
  
  // Handle reboot control
  if (String(topic) == reboot_topic) {
    if (message == "reboot") {
      Serial.println("Reboot command received via MQTT - restarting device...");
      delay(1000); // Give time for the message to be sent
      ESP.restart();
    } else {
      Serial.printf("Invalid reboot command: '%s' (expected 'reboot')\n", message.c_str());
    }
    return;
  }
  
  // Handle brightness control
  if (String(topic) == brightness_topic) {
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 100) {
      setBrightness(brightness);
      
      // Publish status confirmation
      if (mqttConnected) {
        String status = String(displayBrightness);
        mqttClient.publish(brightness_status_topic, status.c_str());
        Serial.printf("Published brightness status: %d%%\n", displayBrightness);
      }
    } else {
      Serial.printf("Invalid brightness value: %d (must be 0-100)\n", brightness);
    }
    return;
  }
}

// Connect to MQTT broker
void connectToMQTT() {
  if (!wifiConnected) return;
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  
  if (!firstImageLoaded) {
    debugPrint("Connecting to MQTT...", YELLOW);
    debugPrintf(WHITE, "Broker: %s:%d", mqtt_server, mqtt_port);
  }
  
  String clientId = String(mqtt_client_id) + "_" + String(random(0xffff), HEX);
  
  bool connected = false;
  if (strlen(mqtt_user) > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }
  
  if (connected) {
    mqttConnected = true;
    Serial.printf("MQTT connected as: %s\n", clientId.c_str());
    
    // Subscribe to reboot control topic
    if (mqttClient.subscribe(reboot_topic)) {
      Serial.printf("Subscribed to topic: %s\n", reboot_topic);
      if (!firstImageLoaded) {
        debugPrint("MQTT connected!", GREEN);
        debugPrintf(WHITE, "Subscribed to: %s", reboot_topic);
      }
    } else {
      Serial.println("Failed to subscribe to reboot topic");
      if (!firstImageLoaded) {
        debugPrint("MQTT subscription failed!", RED);
      }
    }
    
    // Subscribe to brightness control topic
    if (mqttClient.subscribe(brightness_topic)) {
      Serial.printf("Subscribed to brightness topic: %s\n", brightness_topic);
      if (!firstImageLoaded) {
        debugPrintf(WHITE, "Brightness MQTT ready");
      }
    } else {
      Serial.println("Failed to subscribe to brightness topic");
    }
  } else {
    mqttConnected = false;
    Serial.printf("MQTT connection failed, state: %d\n", mqttClient.state());
    if (!firstImageLoaded) {
      debugPrintf(RED, "MQTT failed, state: %d", mqttClient.state());
    }
  }
}

// Handle MQTT reconnection
void handleMQTT() {
  if (!wifiConnected) {
    mqttConnected = false;
    return;
  }
  
  if (!mqttClient.connected()) {
    if (mqttConnected) {
      mqttConnected = false;
      Serial.println("MQTT disconnected");
    }
    
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > mqttReconnectInterval) {
      lastMqttReconnectAttempt = now;
      connectToMQTT();
    }
  } else {
    mqttClient.loop();
  }
}

// Function to render the full image with transformations
void renderFullImage() {
  renderFullImageWithTransition(false);
}

// Function to render the full image with optional smooth transition
void renderFullImageWithTransition(bool useTransition) {
  if (!fullImageBuffer || fullImageWidth == 0 || fullImageHeight == 0) return;
  
  // Calculate final scaled dimensions (accounting for rotation)
  int16_t scaledWidth, scaledHeight;
  if (rotationAngle == 90.0 || rotationAngle == 270.0) {
    // For 90°/270° rotation, dimensions are swapped
    scaledWidth = (int16_t)(fullImageHeight * scaleX);
    scaledHeight = (int16_t)(fullImageWidth * scaleY);
  } else {
    // For 0°/180° rotation, dimensions remain the same
    scaledWidth = (int16_t)(fullImageWidth * scaleX);
    scaledHeight = (int16_t)(fullImageHeight * scaleY);
  }
  
  // Debug output to understand buffer usage
  size_t scaledImageSize = scaledWidth * scaledHeight * 2;
  Serial.printf("DEBUG: Scaled+rotated image %dx%d = %d bytes, buffer = %d bytes, rotation = %.0f°\n", 
               scaledWidth, scaledHeight, scaledImageSize, scaledBufferSize, rotationAngle);
  Serial.printf("DEBUG: PPA available: %s\n", ppa_available ? "YES" : "NO");
  
  // Calculate center position using full display area (no status display)
  int16_t centerX = w / 2;
  int16_t centerY = h / 2;
  
  // Calculate final position with centering and offset
  int16_t finalX = centerX - (scaledWidth / 2) + offsetX;
  int16_t finalY = centerY - (scaledHeight / 2) + offsetY;
  
  // Only clear screen if not using transition (for manual controls)
  if (!useTransition) {
    gfx->fillScreen(BLACK);
  }
  
  if (scaleX == 1.0 && scaleY == 1.0 && rotationAngle == 0.0) {
    // No scaling or rotation needed, direct copy
    Serial.println("DEBUG: Using direct copy (no scaling or rotation)");
    gfx->draw16bitRGBBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
  } else {
    bool hardwareProcessed = false;
    
    // Try hardware acceleration first if available and image fits
    if (ppa_available && scaledImageSize <= scaledBufferSize) {
      Serial.println("DEBUG: Attempting PPA hardware scale+rotate");
      unsigned long hwStart = millis();
      hardwareProcessed = scaleRotateImagePPA(fullImageBuffer, fullImageWidth, fullImageHeight,
                                              scaledBuffer, scaledWidth, scaledHeight, rotationAngle);
      if (hardwareProcessed) {
        unsigned long hwTime = millis() - hwStart;
        Serial.printf("PPA scale+rotate: %lums (%dx%d -> %dx%d, %.0f°)\n", 
                     hwTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, rotationAngle);
        
        // Draw the hardware-processed image
        gfx->draw16bitRGBBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
      } else {
        Serial.println("DEBUG: PPA scale+rotate failed, falling back to software");
      }
    } else {
      Serial.printf("DEBUG: PPA not available or image too large (%d > %d)\n", 
                   scaledImageSize, scaledBufferSize);
    }
    
    // Fall back to software processing if hardware acceleration failed or unavailable
    if (!hardwareProcessed) {
      // For software fallback, we'll implement rotation + scaling
      // Note: This is a simplified software implementation
      // For production, you might want more sophisticated software rotation algorithms
      
      if (rotationAngle == 0.0) {
        // No rotation, just scaling
        if (scaledImageSize <= scaledBufferSize) {
          Serial.println("DEBUG: Using full software scaling (no rotation)");
          unsigned long swStart = millis();
          scaleImageChunk(fullImageBuffer, fullImageWidth, fullImageHeight,
                          scaledBuffer, scaledWidth, scaledHeight);
          unsigned long swTime = millis() - swStart;
          Serial.printf("Software scaling: %lums (%dx%d -> %dx%d)\n", 
                       swTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
          
          gfx->draw16bitRGBBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
        } else {
          // Strip-based software scaling (existing implementation)
          Serial.printf("Strip-based scaling (%dx%d -> %dx%d)\n", 
                       fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
          
          int16_t maxStripHeight = scaledBufferSize / (scaledWidth * 2);
          if (maxStripHeight < 1) maxStripHeight = 1;
          if (maxStripHeight > 64) maxStripHeight = 64;
          
          for (int16_t stripY = 0; stripY < scaledHeight; stripY += maxStripHeight) {
            int16_t currentStripHeight = min((int16_t)maxStripHeight, (int16_t)(scaledHeight - stripY));
            
            // Use legacy strip-based scaling (will replace with optimized version later)
            scaleImageStripLegacy(fullImageBuffer, fullImageWidth, fullImageHeight,
                                scaledBuffer, scaledWidth, scaledHeight,
                                scaleX, scaleY, stripY, currentStripHeight);
            
            int16_t drawX = finalX;
            int16_t drawY = finalY + stripY;
            
            if (drawY < h && drawY + currentStripHeight > 0 && 
                drawX < w && drawX + scaledWidth > 0) {
              gfx->draw16bitRGBBitmap(drawX, drawY, scaledBuffer, scaledWidth, currentStripHeight);
            }
          }
        }
      } else {
        // Software rotation + scaling - simplified implementation
        Serial.printf("Software rotation+scaling: %.0f° (%dx%d -> %dx%d)\n", 
                     rotationAngle, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
        
        // For now, fall back to PPA with no rotation if software rotation is needed
        // This ensures the system remains functional while providing a path for future enhancement
        Serial.println("WARNING: Software rotation not fully implemented, using scaling only");
        
        if (scaledImageSize <= scaledBufferSize) {
          // Calculate dimensions without rotation for fallback
          int16_t fallbackWidth = (int16_t)(fullImageWidth * scaleX);
          int16_t fallbackHeight = (int16_t)(fullImageHeight * scaleY);
          
          scaleImageChunk(fullImageBuffer, fullImageWidth, fullImageHeight,
                          scaledBuffer, fallbackWidth, fallbackHeight);
          
          // Adjust position for fallback dimensions
          int16_t fallbackX = centerX - (fallbackWidth / 2) + offsetX;
          int16_t fallbackY = centerY - (fallbackHeight / 2) + offsetY;
          
          gfx->draw16bitRGBBitmap(fallbackX, fallbackY, scaledBuffer, fallbackWidth, fallbackHeight);
        }
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000); // Give serial time to initialize
  
  Serial.println("=== ESP32-P4 Image Display Starting ===");
  
  // Initialize watchdog timer for system stability
  Serial.println("Configuring watchdog timer...");
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,  // 30 second timeout
    .idle_core_mask = 0,  // Don't monitor idle tasks
    .trigger_panic = false // Don't panic on timeout, just reset
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL); // Add current task to watchdog
  Serial.println("Watchdog timer configured");
  
  // Initialize memory tracking
  minFreeHeap = ESP.getFreeHeap();
  minFreePsram = ESP.getFreePsram();
  Serial.printf("Initial memory - Heap: %d, PSRAM: %d\n", minFreeHeap, minFreePsram);
  
  Serial.println("Initializing display hardware...");
  
  // Initialize display objects
  dsipanel = new Arduino_ESP32DSIPanel(
    display_cfg.hsync_pulse_width,
    display_cfg.hsync_back_porch,
    display_cfg.hsync_front_porch,
    display_cfg.vsync_pulse_width,
    display_cfg.vsync_back_porch,
    display_cfg.vsync_front_porch,
    display_cfg.prefer_speed,
    display_cfg.lane_bit_rate);
  
  if (!dsipanel) {
    Serial.println("ERROR: Failed to create DSI panel!");
    while(1) delay(1000);
  }
  Serial.println("DSI panel created successfully");
  
  gfx = new Arduino_DSI_Display(
    display_cfg.width,
    display_cfg.height,
    dsipanel,
    0,
    true,
    display_cfg.lcd_rst,
    display_cfg.init_cmds,
    display_cfg.init_cmds_size);
  
  if (!gfx) {
    Serial.println("ERROR: Failed to create display object!");
    while(1) delay(1000);
  }
  Serial.println("Display object created successfully");
  
#ifdef GFX_EXTRA_PRE_INIT
  Serial.println("Running GFX_EXTRA_PRE_INIT");
  GFX_EXTRA_PRE_INIT();
#endif

  Serial.println("Starting display initialization...");
  if (!gfx->begin()) {
    Serial.println("ERROR: Display init failed!");
    while(1) delay(1000);
  }
  Serial.println("Display initialized successfully!");
  
  w = gfx->width();
  h = gfx->height();
  Serial.print("Display size: ");
  Serial.print(w);
  Serial.print("x");
  Serial.println(h);
  
  // Clear screen and start debug output
  gfx->fillScreen(BLACK);
  debugY = 50;
  debugLineCount = 0;
  
  // Now we can use debugPrint functions
  debugPrint("=== ESP32 AllSky Display ===", CYAN);
  debugPrint("Display initialized!", GREEN);
  debugPrintf(WHITE, "Display: %dx%d pixels", w, h);
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
  debugPrintf(WHITE, "Free PSRAM: %d bytes", ESP.getFreePsram());
  
  // Allocate image buffer in PSRAM
  imageBufferSize = w * h * 2; // 16-bit color
  debugPrintf(WHITE, "Allocating buffer: %d bytes", imageBufferSize);
  
  imageBuffer = (uint8_t*)ps_malloc(imageBufferSize);
  if (!imageBuffer) {
    debugPrint("ERROR: Memory allocation failed!", RED);
    while(1) delay(1000);
  }
  
  // Allocate full image buffer for smooth rendering (estimate max image size)
  fullImageBufferSize = 1024 * 1024; // 1MB for full image buffer (512x512 max image at 16-bit)
  debugPrintf(WHITE, "Allocating full image buffer: %d bytes", fullImageBufferSize);
  
  fullImageBuffer = (uint16_t*)ps_malloc(fullImageBufferSize);
  if (!fullImageBuffer) {
    debugPrint("ERROR: Full image buffer allocation failed!", RED);
    while(1) delay(1000);
  }
  
  // Allocate scaling buffer for full scaled images (with extra space for scaling)
  scaledBufferSize = w * h * 2 * 4; // 4x display size to handle large scale factors
  debugPrintf(WHITE, "Allocating scaling buffer: %d bytes", scaledBufferSize);
  
  scaledBuffer = (uint16_t*)ps_malloc(scaledBufferSize);
  if (!scaledBuffer) {
    debugPrint("ERROR: Scaling buffer allocation failed!", RED);
    while(1) delay(1000);
  }
  
  debugPrint("Memory allocated successfully", GREEN);
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
  debugPrintf(WHITE, "Free PSRAM: %d bytes", ESP.getFreePsram());
  
  // Initialize PPA hardware acceleration
  ppa_available = initPPA();
  if (ppa_available) {
  debugPrint("Hardware acceleration enabled!", GREEN);
  } else {
    debugPrint("Using software scaling fallback", YELLOW);
  }
  
  // Initialize brightness control
  initBrightness();
  
  // Connect to WiFi
  debugPrint("Starting WiFi connection...", YELLOW);
  connectToWiFi();
  
  // Initialize OTA web server after WiFi is established
  if (wifiConnected) {
    debugPrint("Initializing OTA server...", YELLOW);
    
    // Set up a simple root page
    server.on("/", []() {
      server.send(200, "text/plain", "ESP32-P4 Allsky Display - OTA Ready");
    });
    
    // Initialize ElegantOTA
    ElegantOTA.begin(&server);
    
    // Set up ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
    
    // Start the web server
    server.begin();
    
    debugPrint("OTA server started!", GREEN);
    debugPrintf(WHITE, "OTA URL: http://%s/update", WiFi.localIP().toString().c_str());
    
    // Connect to MQTT after WiFi is established
    connectToMQTT();
  }
  
  debugPrint("Setup complete!", GREEN);
  delay(1000);
}

void connectToWiFi() {
  // Validate WiFi credentials first
  if (strlen(ssid) == 0) {
    debugPrint("ERROR: WiFi SSID is empty!", RED);
    wifiConnected = false;
    return;
  }
  
  debugPrint("Setting WiFi mode to STA");
  WiFi.mode(WIFI_STA);
  
  debugPrintf(WHITE, "Connecting to: %s", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  const int maxAttempts = 15; // Reduced from 20 to prevent long blocking
  unsigned long startTime = millis();
  const unsigned long maxWaitTime = 12000; // 12 seconds maximum wait time
  
  debugPrint("WiFi connecting", YELLOW);
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    // Check for timeout to prevent infinite hanging
    if (millis() - startTime > maxWaitTime) {
      debugPrint("WiFi connection timeout!", RED);
      break;
    }
    
    safeDelay(400);  // Reduced delay for faster response
    resetWatchdog(); // Explicitly reset watchdog during connection attempts
    attempts++;
    
    // Show progress dots
    if (!firstImageLoaded) {
      gfx->setTextColor(YELLOW);
      gfx->print(".");
    }
    
    if (attempts % 4 == 0) {  // More frequent status updates
      debugPrintf(YELLOW, "Attempt %d/%d...", attempts, maxAttempts);
      resetWatchdog(); // Additional watchdog reset
    }
    
    // Allow other tasks to run
    yield();
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    debugPrint("WiFi connected!", GREEN);
    debugPrintf(GREEN, "IP: %s", WiFi.localIP().toString().c_str());
    debugPrintf(WHITE, "Signal: %d dBm", WiFi.RSSI());
    debugPrintf(WHITE, "MAC: %s", WiFi.macAddress().c_str());
  } else {
    wifiConnected = false;
    debugPrintf(RED, "WiFi failed after %d attempts", attempts);
    debugPrintf(RED, "Status code: %d", WiFi.status());
    debugPrint("Will retry in main loop...", YELLOW);
    
    // Disconnect to clean up any partial connection state
    WiFi.disconnect();
    safeDelay(1000);
  }
}

void downloadAndDisplayImage() {
  if (!wifiConnected) {
    debugPrint("ERROR: No WiFi connection", RED);
    return;
  }
  
  resetWatchdog(); // Reset watchdog before starting download
  
  debugPrint("=== Starting Download ===", CYAN);
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
  debugPrintf(WHITE, "URL: %s", imageURL);
  
  HTTPClient http;
  http.begin(imageURL);
  http.setTimeout(10000);
  
  debugPrint("Sending HTTP request...");
  unsigned long downloadStart = millis();
  resetWatchdog(); // Reset before HTTP request
  int httpCode = http.GET();
  unsigned long downloadTime = millis() - downloadStart;
  
  debugPrintf(WHITE, "HTTP code: %d (%lu ms)", httpCode, downloadTime);
  
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    
    debugPrintf(WHITE, "Image size: %d bytes", size);
    
    if (size > 0 && size < imageBufferSize) {
      debugPrint("Downloading image data...", YELLOW);
      
      size_t bytesRead = 0;
      uint8_t* buffer = imageBuffer;
      unsigned long readStart = millis();
      
      while (http.connected() && bytesRead < size) {
        resetWatchdog(); // Reset watchdog during download
        size_t available = stream->available();
        if (available) {
          size_t toRead = min(available, size - bytesRead);
          size_t read = stream->readBytes(buffer + bytesRead, toRead);
          bytesRead += read;
          
          // Show progress every 20KB and reset watchdog
          if (bytesRead % 20480 == 0) {
            debugPrintf(YELLOW, "Downloaded: %.1f%%", (float)bytesRead/size*100);
            resetWatchdog();
          }
        }
        safeYield(); // Use safe yield instead of delay(1)
      }
      
      unsigned long readTime = millis() - readStart;
      debugPrintf(GREEN, "Download complete: %d bytes", bytesRead);
      debugPrintf(WHITE, "Speed: %.2f KB/s", (float)bytesRead/readTime);
      
      // Decode and display JPEG
      debugPrint("Decoding JPEG...", YELLOW);
      
      if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
        // Store original image dimensions and size
        originalImageWidth = jpeg.getWidth();
        originalImageHeight = jpeg.getHeight();
        currentImageSize = bytesRead;
        
        debugPrintf(WHITE, "JPEG: %dx%d pixels", originalImageWidth, originalImageHeight);
        
        // Check if image fits in our full image buffer
        if (originalImageWidth * originalImageHeight * 2 <= fullImageBufferSize) {
          debugPrint("Decoding to full buffer...", YELLOW);
          
          // Set up full image buffer dimensions
          fullImageWidth = originalImageWidth;
          fullImageHeight = originalImageHeight;
          
          // Clear the full image buffer
          memset(fullImageBuffer, 0, fullImageWidth * fullImageHeight * sizeof(uint16_t));
          
          // Decode the full image into our buffer
          jpeg.decode(0, 0, 0);
          jpeg.close();
          
          debugPrint("Rendering image...", GREEN);
          
          // Now render the full image with transformations
          renderFullImage();
          
          // Mark first image as loaded
          firstImageLoaded = true;
          
          debugPrint("SUCCESS: Image displayed!", GREEN);
          Serial.println("First image loaded successfully - switching to image mode");
        } else {
          debugPrint("ERROR: Image too large for buffer!", RED);
          jpeg.close();
        }
        
      } else {
        debugPrint("ERROR: JPEG decode failed!", RED);
      }
    } else {
      debugPrintf(RED, "Invalid size: %d bytes", size);
    }
  } else {
    debugPrintf(RED, "HTTP error: %d", httpCode);
    String errorMsg = http.errorToString(httpCode);
    debugPrintf(RED, "Error: %s", errorMsg.c_str());
  }
  
  http.end();
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
}

void downloadAndDisplayImageSilent() {
  // Silent version for subsequent downloads (no debug output)
  if (!wifiConnected) return;
  
  resetWatchdog(); // Reset watchdog before starting download
  
  // Check system health before proceeding
  if (!systemHealthy) {
    Serial.println("Skipping image update due to poor system health");
    return;
  }
  
  HTTPClient http;
  http.begin(imageURL);
  http.setTimeout(10000);
  
  // First, do a HEAD request to check if image has changed
  resetWatchdog(); // Reset before HTTP request
  int httpCode = http.sendRequest("HEAD");
  
  if (httpCode == HTTP_CODE_OK) {
    // Check ETag and Last-Modified headers
    String etag = http.header("ETag");
    String lastModifiedStr = http.header("Last-Modified");
    size_t contentLength = http.getSize();
    
    // Check if image has actually changed
    bool imageChanged = false;
    unsigned long currentTime = millis();
    
    // Force check every 5 minutes regardless of cache headers (for servers with poor caching)
    bool forceCheck = (currentTime - lastSuccessfulCheck) > forceCheckInterval;
    
    if (etag.length() > 0 && etag != lastETag) {
      imageChanged = true;
      lastETag = etag;
      Serial.printf("Image changed (ETag): %s\n", etag.c_str());
    } else if (contentLength != lastImageSize && contentLength > 0) {
      imageChanged = true;
      lastImageSize = contentLength;
      Serial.printf("Image changed (size): %d bytes\n", contentLength);
    } else if (forceCheck) {
      // Force periodic check for servers that don't update cache headers properly
      imageChanged = true;
      lastSuccessfulCheck = currentTime;
      Serial.printf("Forced check after %lu minutes (cache headers may be unreliable)\n", 
                   forceCheckInterval / 60000);
    } else if (etag.length() == 0 && lastModifiedStr.length() == 0) {
      // No cache headers available, assume it might have changed
      imageChanged = true;
      Serial.println("No cache headers, assuming image changed");
    }
    
    http.end();
    
    if (!imageChanged) {
      Serial.println("Image unchanged, skipping download");
      return;
    }
    
    // Image has changed, proceed with full download
    Serial.println("Image changed, downloading...");
    http.begin(imageURL);
    http.setTimeout(10000);
    resetWatchdog();
    httpCode = http.GET();
  }
  
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    
    if (size > 0 && size < imageBufferSize) {
      size_t bytesRead = 0;
      uint8_t* buffer = imageBuffer;
      
      while (http.connected() && bytesRead < size) {
        resetWatchdog(); // Reset watchdog during download
        size_t available = stream->available();
        if (available) {
          size_t toRead = min(available, size - bytesRead);
          size_t read = stream->readBytes(buffer + bytesRead, toRead);
          bytesRead += read;
          
          // Reset watchdog every 20KB
          if (bytesRead % 20480 == 0) {
            resetWatchdog();
          }
        }
        safeYield(); // Use safe yield instead of delay(1)
      }
      
      resetWatchdog(); // Reset before JPEG processing
      
      // Decode and display JPEG
      if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
        // Update original image dimensions and size
        originalImageWidth = jpeg.getWidth();
        originalImageHeight = jpeg.getHeight();
        currentImageSize = bytesRead;
        lastImageSize = bytesRead; // Update our tracking
        
        // Check if image fits in our full image buffer
        if (originalImageWidth * originalImageHeight * 2 <= fullImageBufferSize) {
          // Set up full image buffer dimensions
          fullImageWidth = originalImageWidth;
          fullImageHeight = originalImageHeight;
          
          // Clear the full image buffer
          memset(fullImageBuffer, 0, fullImageWidth * fullImageHeight * sizeof(uint16_t));
          
          resetWatchdog(); // Reset before JPEG decode
          
          // Decode the full image into our buffer
          jpeg.decode(0, 0, 0);
          jpeg.close();
          
          resetWatchdog(); // Reset before rendering
          
          // Now render the full image with smooth transition (single clean update)
          renderFullImageWithTransition(true);
          Serial.printf("New image displayed (%dx%d, %d bytes)\n", 
                       originalImageWidth, originalImageHeight, bytesRead);
        } else {
          Serial.printf("WARNING: Image too large for buffer (%dx%d)\n", originalImageWidth, originalImageHeight);
          jpeg.close();
        }
      } else {
        Serial.println("WARNING: Silent JPEG decode failed");
      }
    } else {
      Serial.printf("WARNING: Invalid image size: %d bytes\n", size);
    }
  } else {
    Serial.printf("WARNING: HTTP error in silent download: %d\n", httpCode);
  }
  
  http.end();
  resetWatchdog(); // Reset after download complete
}

// Image control functions
void scaleImageX(float factor) {
  scaleX = constrain(scaleX + factor, MIN_SCALE, MAX_SCALE);
  redrawImage();
}

void scaleImageY(float factor) {
  scaleY = constrain(scaleY + factor, MIN_SCALE, MAX_SCALE);
  redrawImage();
}

void scaleImageBoth(float factor) {
  scaleX = constrain(scaleX + factor, MIN_SCALE, MAX_SCALE);
  scaleY = constrain(scaleY + factor, MIN_SCALE, MAX_SCALE);
  redrawImage();
}

void moveImage(int16_t deltaX, int16_t deltaY) {
  offsetX += deltaX;
  offsetY += deltaY;
  redrawImage();
}

void rotateImage(float deltaAngle) {
  rotationAngle += deltaAngle;
  
  // Normalize rotation angle to 0-360 range
  while (rotationAngle < 0) rotationAngle += 360.0;
  while (rotationAngle >= 360.0) rotationAngle -= 360.0;
  
  // Snap to nearest 90-degree increment for hardware acceleration
  if (rotationAngle >= 0 && rotationAngle < 45) rotationAngle = 0;
  else if (rotationAngle >= 45 && rotationAngle < 135) rotationAngle = 90;
  else if (rotationAngle >= 135 && rotationAngle < 225) rotationAngle = 180;
  else if (rotationAngle >= 225 && rotationAngle < 315) rotationAngle = 270;
  else rotationAngle = 0;
  
  redrawImage();
}

void toggleRotation180() {
  if (rotationAngle == 0.0 || rotationAngle == 90.0 || rotationAngle == 270.0) {
    rotationAngle = 180.0;
  } else {
    rotationAngle = 0.0;
  }
  redrawImage();
}

void resetImageTransform() {
  scaleX = 1.0;
  scaleY = 1.0;
  offsetX = 0;
  offsetY = 0;
  rotationAngle = 0.0;
  redrawImage();
}

void redrawImage() {
  if (!firstImageLoaded || !fullImageBuffer || fullImageWidth == 0 || fullImageHeight == 0) return;
  
  // Use the existing full image buffer to render with new transformations
  renderFullImage();
}


void processSerialCommands() {
  if (Serial.available()) {
    char command = Serial.read();
    
    switch (command) {
      // Scaling commands
      case '+':
        scaleImageBoth(SCALE_STEP);
        Serial.printf("Scale both: %.1fx%.1f\n", scaleX, scaleY);
        break;
      case '-':
        scaleImageBoth(-SCALE_STEP);
        Serial.printf("Scale both: %.1fx%.1f\n", scaleX, scaleY);
        break;
      case 'X':
      case 'x':
        scaleImageX(SCALE_STEP);
        Serial.printf("Scale X: %.1f\n", scaleX);
        break;
      case 'Y':
      case 'y':
        scaleImageY(SCALE_STEP);
        Serial.printf("Scale Y: %.1f\n", scaleY);
        break;
      case 'Z':
      case 'z':
        scaleImageX(-SCALE_STEP);
        Serial.printf("Scale X: %.1f\n", scaleX);
        break;
      case 'U':
      case 'u':
        scaleImageY(-SCALE_STEP);
        Serial.printf("Scale Y: %.1f\n", scaleY);
        break;
        
      // Movement commands
      case 'W':
      case 'w':
        moveImage(0, -MOVE_STEP);
        Serial.printf("Move up, offset: %d,%d\n", offsetX, offsetY);
        break;
      case 'S':
      case 's':
        moveImage(0, MOVE_STEP);
        Serial.printf("Move down, offset: %d,%d\n", offsetX, offsetY);
        break;
      case 'A':
      case 'a':
        moveImage(-MOVE_STEP, 0);
        Serial.printf("Move left, offset: %d,%d\n", offsetX, offsetY);
        break;
      case 'D':
      case 'd':
        moveImage(MOVE_STEP, 0);
        Serial.printf("Move right, offset: %d,%d\n", offsetX, offsetY);
        break;
        
      // Rotation commands
      case 'Q':
      case 'q':
        rotateImage(-ROTATION_STEP);
        Serial.printf("Rotate CCW: %.0f°\n", rotationAngle);
        break;
      case 'E':
      case 'e':
        rotateImage(ROTATION_STEP);
        Serial.printf("Rotate CW: %.0f°\n", rotationAngle);
        break;
      case 'T':
      case 't':
        toggleRotation180();
        Serial.printf("Toggle 180°: %.0f°\n", rotationAngle);
        break;
      case 'O':
      case 'o':
        rotationAngle = 0.0;
        redrawImage();
        Serial.printf("Reset rotation: %.0f°\n", rotationAngle);
        break;
        
      // Reset command
      case 'R':
      case 'r':
        resetImageTransform();
        Serial.println("Reset all transformations");
        break;
        
      // Reboot command
      case 'B':
      case 'b':
        Serial.println("Reboot command received - restarting device in 2 seconds...");
        Serial.println("WARNING: Device will restart now!");
        Serial.flush(); // Ensure message is sent before restart
        delay(2000); // Give user time to see the message
        ESP.restart();
        break;
        
      // Help command
      case 'H':
      case 'h':
      case '?':
        Serial.println("\n=== Image Control Commands ===");
        Serial.println("Scaling:");
        Serial.println("  +/- : Scale both axes");
        Serial.println("  X/Z : Scale X-axis up/down");
        Serial.println("  Y/U : Scale Y-axis up/down");
        Serial.println("Movement:");
        Serial.println("  W/S : Move up/down");
        Serial.println("  A/D : Move left/right");
        Serial.println("Rotation:");
        Serial.println("  Q/E : Rotate 90° CCW/CW");
        Serial.println("  T   : Toggle 180° rotation");
        Serial.println("  O   : Reset rotation to 0°");
        Serial.println("Reset:");
        Serial.println("  R   : Reset all transformations");
        Serial.println("Brightness:");
        Serial.println("  L/K : Brightness up/down (±10%)");
        Serial.println("  M   : Show current brightness");
        Serial.println("System:");
        Serial.println("  B   : Reboot device");
        Serial.println("Help:");
        Serial.println("  H/? : Show this help");
        Serial.printf("Current: Scale %.1fx%.1f, Offset %d,%d, Rotation %.0f°\n", 
                     scaleX, scaleY, offsetX, offsetY, rotationAngle);
        Serial.printf("Brightness: %d%%\n", displayBrightness);
        break;
        
      // Brightness commands
      case 'L':
      case 'l':
        setBrightness(min(displayBrightness + 10, 100));
        Serial.printf("Brightness up: %d%%\n", displayBrightness);
        break;
        
      case 'K':
      case 'k':
        setBrightness(max(displayBrightness - 10, 0));
        Serial.printf("Brightness down: %d%%\n", displayBrightness);
        break;
        
      case 'M':
      case 'm':
        Serial.printf("Current brightness: %d%%\n", displayBrightness);
        break;
        
      default:
        // Ignore unknown commands
        break;
    }
    
    // Clear any remaining characters
    while (Serial.available()) {
      Serial.read();
    }
  }
}

// Add a flag to prevent image processing loops
bool imageProcessing = false;
unsigned long lastImageProcessTime = 0;
const unsigned long imageProcessTimeout = 5000; // 5 second timeout for image processing

void loop() {
  // Force immediate watchdog reset at start of each loop
  forceResetWatchdog();
  checkSystemHealth();
  flushSerial();
  
  // Check for critical system health issues
  if (!systemHealthy) {
    Serial.println("CRITICAL: System health compromised, attempting recovery...");
    forceResetWatchdog(); // Force reset before delay
    safeDelay(5000); // Wait before continuing
    forceResetWatchdog(); // Force reset after delay
  }
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      if (!firstImageLoaded) {
        debugPrint("ERROR: WiFi disconnected!", RED);
        debugPrint("Attempting reconnection...", YELLOW);
      }
      forceResetWatchdog(); // Force reset before WiFi operations
      connectToWiFi();
    }
    forceResetWatchdog(); // Force reset after WiFi operations
    return;
  } else if (!wifiConnected) {
    wifiConnected = true;
    if (!firstImageLoaded) {
      debugPrint("WiFi reconnected!", GREEN);
    }
  }
  
  // Handle MQTT connection and messages
  forceResetWatchdog(); // Force reset before MQTT
  handleMQTT();
  forceResetWatchdog(); // Force reset after MQTT operations
  
  // Handle OTA web server
  server.handleClient();
  ElegantOTA.loop();
  forceResetWatchdog(); // Force reset after OTA operations
  
  // Process serial commands for image control
  processSerialCommands();
  forceResetWatchdog(); // Force reset after serial processing
  
  // Check for stuck image processing
  if (imageProcessing && (millis() - lastImageProcessTime > imageProcessTimeout)) {
    Serial.println("WARNING: Image processing timeout detected, resetting...");
    imageProcessing = false;
    forceResetWatchdog();
  }
  
  // Check if it's time to update the image (only if not currently processing)
  unsigned long currentTime = millis();
  if (!imageProcessing && (currentTime - lastUpdate >= updateInterval || lastUpdate == 0)) {
    imageProcessing = true;
    lastImageProcessTime = currentTime;
    forceResetWatchdog(); // Force reset before image operations
    
    if (!firstImageLoaded) {
      // First download with debug output
      downloadAndDisplayImage();
    } else {
      // Subsequent downloads without debug output
      downloadAndDisplayImageSilent();
    }
    
    lastUpdate = currentTime;
    imageProcessing = false;
    forceResetWatchdog(); // Force reset after image operations
  }
  
  // Additional watchdog reset before delay
  forceResetWatchdog();
  safeDelay(100); // Use safe delay instead of regular delay
  forceResetWatchdog(); // Force reset after delay
}
