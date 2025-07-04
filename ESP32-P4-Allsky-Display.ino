#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <PubSubClient.h>
#include "displays_config.h"

// ESP-IDF includes for PPA hardware acceleration
extern "C" {
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
}

// WiFi Configuration - Update these with your credentials
const char* ssid = "IoT";
const char* password = "kkkkkkkk";

// MQTT Configuration - Update these with your MQTT broker details
const char* mqtt_server = "192.168.1.250";  // Replace with your MQTT broker IP
const int mqtt_port = 1883;
const char* mqtt_user = "";                 // Leave empty if no authentication
const char* mqtt_password = "";             // Leave empty if no authentication
const char* mqtt_client_id = "ESP32_Allsky_Display";
const char* brightness_topic = "Astro/AllSky/display/brightness"; // Topic to subscribe for brightness control

// Image Configuration
const char* imageURL = "https://allsky.challa.co:1982/current/resized/image.jpg"; // Random image service
const unsigned long updateInterval = 60000; // 10 seconds in milliseconds

// Brightness Control Variables
uint8_t displayBrightness = 255;  // Current brightness (0-255)
const uint8_t MIN_BRIGHTNESS = 10; // Minimum brightness to keep display visible
const uint8_t MAX_BRIGHTNESS = 255; // Maximum brightness
bool brightnessChanged = false;    // Flag to indicate brightness change

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

// Debug display variables
int debugY = 50;
const int debugLineHeight = 20;
const int maxDebugLines = 30; // Maximum lines to show on screen
int debugLineCount = 0;
const int debugTextSize = 2;

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

// Bilinear interpolation scaling function
void scaleImageChunk(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                     uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
  float xRatio = (float)srcWidth / dstWidth;
  float yRatio = (float)srcHeight / dstHeight;
  
  for (int16_t y = 0; y < dstHeight; y++) {
    for (int16_t x = 0; x < dstWidth; x++) {
      float srcX = x * xRatio;
      float srcY = y * yRatio;
      
      int16_t x1 = (int16_t)srcX;
      int16_t y1 = (int16_t)srcY;
      int16_t x2 = min(x1 + 1, srcWidth - 1);
      int16_t y2 = min(y1 + 1, srcHeight - 1);
      
      float xWeight = srcX - x1;
      float yWeight = srcY - y1;
      
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
      
      // Bilinear interpolation
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
      
      // Combine back to RGB565
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
  
  // Destination buffer - for scaled image data
  ppa_dst_buffer_size = w * h * 2; // Full display size at 16-bit
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

// Hardware-accelerated image scaling using PPA
bool scaleImagePPA(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                   uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
  if (!ppa_available || !ppa_scaling_handle) {
    return false; // Fall back to software scaling
  }
  
  // Check if source fits in PPA buffer
  size_t srcSize = srcWidth * srcHeight * sizeof(uint16_t);
  if (srcSize > ppa_src_buffer_size) {
    return false; // Too large for hardware acceleration
  }
  
  // Check if destination fits in PPA buffer
  size_t dstSize = dstWidth * dstHeight * sizeof(uint16_t);
  if (dstSize > ppa_dst_buffer_size) {
    return false; // Too large for hardware acceleration
  }
  
  // Copy source data to DMA-aligned buffer
  memcpy(ppa_src_buffer, srcPixels, srcSize);
  
  // Ensure cache coherency for DMA operations
  esp_cache_msync(ppa_src_buffer, srcSize, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
  
  // Configure PPA scaling operation
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
  srm_oper_config.scale_x = (float)srcWidth / dstWidth;
  srm_oper_config.scale_y = (float)srcHeight / dstHeight;
  
  // Rotation configuration (no rotation)
  srm_oper_config.rotation_angle = PPA_SRM_ROTATION_ANGLE_0;
  
  // Perform the scaling operation
  esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_scaling_handle, &srm_oper_config);
  if (ret != ESP_OK) {
    Serial.printf("PPA scaling operation failed: %s\n", esp_err_to_name(ret));
    return false;
  }
  
  // Ensure cache coherency for reading results
  esp_cache_msync(ppa_dst_buffer, dstSize, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  
  // Copy result to destination buffer
  memcpy(dstPixels, ppa_dst_buffer, dstSize);
  
  return true;
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

// Hardware brightness control functions
void setBrightness(uint8_t brightness) {
  displayBrightness = constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
  brightnessChanged = true;
  
  Serial.printf("Brightness set to: %d\n", displayBrightness);
  
  // Apply hardware brightness control
  applyHardwareBrightness();
}

// Apply hardware brightness control via LCD registers
void applyHardwareBrightness() {
  if (!gfx || !dsipanel) return;
  
  // Calculate brightness value for LCD register (0x51 command)
  // Convert from 0-255 range to 0-255 range (direct mapping for this display)
  uint8_t lcdBrightness = displayBrightness;
  
  Serial.printf("Setting hardware brightness to: %d\n", lcdBrightness);
  
  // Send brightness control command to LCD
  // 0x51 is the standard MIPI DCS command for setting display brightness
  lcd_init_cmd_t brightnessCmd = {
    0x51,
    &lcdBrightness,
    1,
    0
  };
  
  // Apply the brightness command
  if (dsipanel) {
    // Use the DSI panel's command sending capability
    // First, we need to enable command mode
    enableDisplayCommandMode();
    
    // Send the brightness command
    sendDisplayCommand(0x51, &lcdBrightness, 1);
    
    // Return to video mode
    enableDisplayVideoMode();
  }
}

// Enable command mode for sending LCD commands
void enableDisplayCommandMode() {
  // This function enables command mode on the DSI interface
  // Implementation depends on the specific DSI controller
  // For ESP32-P4, we need to use the DSI peripheral registers
  
  // Note: This is a simplified implementation
  // In practice, you might need to access DSI registers directly
  Serial.println("Switching to command mode");
}

// Send command to display
void sendDisplayCommand(uint8_t cmd, uint8_t* data, size_t len) {
  // Send DCS command to the display
  // This is a simplified implementation - actual implementation would
  // use the ESP32-P4 DSI peripheral to send the command
  
  Serial.printf("Sending LCD command 0x%02X with %d bytes of data\n", cmd, len);
  
  // For now, we'll use a workaround by creating a temporary command array
  // and trying to send it through the display initialization path
  if (len > 0 && data) {
    lcd_init_cmd_t tempCmd = {
      cmd,
      data,
      (uint8_t)len,
      0
    };
    
    // Try to send the command (this is a simplified approach)
    // In a full implementation, you'd access the DSI registers directly
  }
}

// Enable video mode for normal display operation
void enableDisplayVideoMode() {
  // This function returns the DSI interface to video mode
  Serial.println("Switching to video mode");
}

// Alternative PWM-based backlight control (if available)
void setPWMBrightness(uint8_t brightness) {
  // If your display has a separate backlight control pin, you can use PWM
  // This is an alternative approach if direct LCD brightness control doesn't work
  
  const int backlightPin = 45; // Change this to your actual backlight control pin
  const int pwmChannel = 0;
  const int pwmFreq = 5000;
  const int pwmResolution = 8;
  
  static bool pwmInitialized = false;
  
  if (!pwmInitialized) {
    // Initialize PWM for backlight control (newer ESP32 Arduino core)
    if (ledcAttach(backlightPin, pwmFreq, pwmResolution) == 0) {
      Serial.println("ERROR: Failed to attach LEDC to pin");
      return;
    }
    pwmInitialized = true;
    Serial.printf("PWM backlight initialized on pin %d\n", backlightPin);
  }
  
  // Set PWM duty cycle based on brightness
  ledcWrite(backlightPin, brightness);
  Serial.printf("PWM brightness set to: %d\n", brightness);
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("MQTT message received on topic '%s': %s\n", topic, message.c_str());
  
  // Handle brightness control
  if (String(topic) == brightness_topic) {
    int brightness = message.toInt();
    if (brightness >= 0 && brightness <= 255) {
      setBrightness(brightness);
      Serial.printf("Brightness changed via MQTT to: %d\n", brightness);
    } else {
      Serial.printf("Invalid brightness value: %d (must be 0-255)\n", brightness);
    }
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
    
    // Subscribe to brightness control topic
    if (mqttClient.subscribe(brightness_topic)) {
      Serial.printf("Subscribed to topic: %s\n", brightness_topic);
      if (!firstImageLoaded) {
        debugPrint("MQTT connected!", GREEN);
        debugPrintf(WHITE, "Subscribed to: %s", brightness_topic);
      }
    } else {
      Serial.println("Failed to subscribe to brightness topic");
      if (!firstImageLoaded) {
        debugPrint("MQTT subscription failed!", RED);
      }
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
  
  // Calculate final scaled dimensions
  int16_t scaledWidth = (int16_t)(fullImageWidth * scaleX);
  int16_t scaledHeight = (int16_t)(fullImageHeight * scaleY);
  
  // Debug output to understand buffer usage
  size_t scaledImageSize = scaledWidth * scaledHeight * 2;
  Serial.printf("DEBUG: Scaled image %dx%d = %d bytes, buffer = %d bytes\n", 
               scaledWidth, scaledHeight, scaledImageSize, scaledBufferSize);
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
  
  if (scaleX == 1.0 && scaleY == 1.0) {
    // No scaling needed, direct copy
    Serial.println("DEBUG: Using direct copy (no scaling)");
    gfx->draw16bitRGBBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
  } else {
    bool hardwareScaled = false;
    
    // Try hardware acceleration first if available and image fits
    if (ppa_available && scaledImageSize <= scaledBufferSize) {
      Serial.println("DEBUG: Attempting PPA hardware scaling");
      unsigned long hwStart = millis();
      hardwareScaled = scaleImagePPA(fullImageBuffer, fullImageWidth, fullImageHeight,
                                     scaledBuffer, scaledWidth, scaledHeight);
      if (hardwareScaled) {
        unsigned long hwTime = millis() - hwStart;
        Serial.printf("PPA scaling: %lums (%dx%d -> %dx%d)\n", 
                     hwTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
        
        // Draw the hardware-scaled image
        gfx->draw16bitRGBBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
      } else {
        Serial.println("DEBUG: PPA scaling failed, falling back to software");
      }
    } else {
      Serial.printf("DEBUG: PPA not available or image too large (%d > %d)\n", 
                   scaledImageSize, scaledBufferSize);
    }
    
    // Fall back to software scaling if hardware acceleration failed or unavailable
    if (!hardwareScaled) {
      // Check if we can scale the entire image at once with software
      if (scaledImageSize <= scaledBufferSize) {
        Serial.println("DEBUG: Using full software scaling");
        // Scale the entire image at once for seamless rendering
        unsigned long swStart = millis();
        scaleImageChunk(fullImageBuffer, fullImageWidth, fullImageHeight,
                        scaledBuffer, scaledWidth, scaledHeight);
        unsigned long swTime = millis() - swStart;
        Serial.printf("Software scaling: %lums (%dx%d -> %dx%d)\n", 
                     swTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
        
        // Draw the entire scaled image
        gfx->draw16bitRGBBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
      } else {
        // For large scaled images, render in horizontal strips to avoid slow pixel-by-pixel drawing
        Serial.printf("Strip-based scaling (%dx%d -> %dx%d)\n", 
                     fullImageWidth, fullImageHeight, scaledWidth, scaledHeight);
        
        // Calculate optimal strip height based on available buffer
        int16_t maxStripHeight = scaledBufferSize / (scaledWidth * 2);
        if (maxStripHeight < 1) maxStripHeight = 1;
        if (maxStripHeight > 64) maxStripHeight = 64; // Limit to reasonable chunk size
        
        Serial.printf("DEBUG: Strip height = %d pixels\n", maxStripHeight);
        
        // Process image in horizontal strips
        for (int16_t stripY = 0; stripY < scaledHeight; stripY += maxStripHeight) {
          int16_t currentStripHeight = min((int16_t)maxStripHeight, (int16_t)(scaledHeight - stripY));
          
          // Scale this strip
          for (int16_t y = 0; y < currentStripHeight; y++) {
            int16_t dstY = stripY + y;
            float srcYFloat = (float)dstY / scaleY;
            int16_t srcY = (int16_t)srcYFloat;
            if (srcY >= fullImageHeight - 1) srcY = fullImageHeight - 1;
            float yWeight = srcYFloat - srcY;
            
            for (int16_t dstX = 0; dstX < scaledWidth; dstX++) {
              float srcXFloat = (float)dstX / scaleX;
              int16_t srcX = (int16_t)srcXFloat;
              if (srcX >= fullImageWidth - 1) srcX = fullImageWidth - 1;
              float xWeight = srcXFloat - srcX;
              
              // Get the four surrounding pixels for bilinear interpolation
              uint16_t p1 = fullImageBuffer[srcY * fullImageWidth + srcX];
              uint16_t p2 = fullImageBuffer[srcY * fullImageWidth + min(srcX + 1, fullImageWidth - 1)];
              uint16_t p3 = fullImageBuffer[min(srcY + 1, fullImageHeight - 1) * fullImageWidth + srcX];
              uint16_t p4 = fullImageBuffer[min(srcY + 1, fullImageHeight - 1) * fullImageWidth + min(srcX + 1, fullImageWidth - 1)];
              
              // Extract RGB components (RGB565 format)
              uint8_t r1 = (p1 >> 11) & 0x1F, g1 = (p1 >> 5) & 0x3F, b1 = p1 & 0x1F;
              uint8_t r2 = (p2 >> 11) & 0x1F, g2 = (p2 >> 5) & 0x3F, b2 = p2 & 0x1F;
              uint8_t r3 = (p3 >> 11) & 0x1F, g3 = (p3 >> 5) & 0x3F, b3 = p3 & 0x1F;
              uint8_t r4 = (p4 >> 11) & 0x1F, g4 = (p4 >> 5) & 0x3F, b4 = p4 & 0x1F;
              
              // Bilinear interpolation
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
              
              // Store in strip buffer
              uint16_t finalPixel = ((uint16_t)(r + 0.5) << 11) |
                                   ((uint16_t)(g + 0.5) << 5) |
                                   (uint16_t)(b + 0.5);
              
              scaledBuffer[y * scaledWidth + dstX] = finalPixel;
            }
          }
          
          // Draw the entire strip at once for smooth rendering
          int16_t drawX = finalX;
          int16_t drawY = finalY + stripY;
          
          // Only draw if the strip is visible on screen
          if (drawY < h && drawY + currentStripHeight > 0 && 
              drawX < w && drawX + scaledWidth > 0) {
            gfx->draw16bitRGBBitmap(drawX, drawY, scaledBuffer, scaledWidth, currentStripHeight);
          }
        }
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000); // Give serial time to initialize
  
  Serial.println("=== ESP32-P4 Image Display Starting ===");
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
  debugPrint("=== ESP32 Image Display ===", CYAN);
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
  
  // Connect to WiFi
  debugPrint("Starting WiFi connection...", YELLOW);
  connectToWiFi();
  
  // Connect to MQTT after WiFi is established
  if (wifiConnected) {
    connectToMQTT();
  }
  
  debugPrint("Setup complete!", GREEN);
  delay(1000);
}

void connectToWiFi() {
  debugPrint("Setting WiFi mode to STA");
  WiFi.mode(WIFI_STA);
  
  debugPrintf(WHITE, "Connecting to: %s", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  debugPrint("WiFi connecting", YELLOW);
  
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    
    // Show progress dots
    gfx->setTextColor(YELLOW);
    gfx->print(".");
    
    if (attempts % 10 == 0) {
      debugPrintf(YELLOW, "Attempt %d/20...", attempts);
    }
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
  }
}

void downloadAndDisplayImage() {
  if (!wifiConnected) {
    debugPrint("ERROR: No WiFi connection", RED);
    return;
  }
  
  debugPrint("=== Starting Download ===", CYAN);
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
  debugPrintf(WHITE, "URL: %s", imageURL);
  
  HTTPClient http;
  http.begin(imageURL);
  http.setTimeout(10000);
  
  debugPrint("Sending HTTP request...");
  unsigned long downloadStart = millis();
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
        size_t available = stream->available();
        if (available) {
          size_t toRead = min(available, size - bytesRead);
          size_t read = stream->readBytes(buffer + bytesRead, toRead);
          bytesRead += read;
          
          // Show progress every 20KB
          if (bytesRead % 20480 == 0) {
            debugPrintf(YELLOW, "Downloaded: %.1f%%", (float)bytesRead/size*100);
          }
        }
        delay(1);
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
  
  HTTPClient http;
  http.begin(imageURL);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    
    if (size > 0 && size < imageBufferSize) {
      size_t bytesRead = 0;
      uint8_t* buffer = imageBuffer;
      
      while (http.connected() && bytesRead < size) {
        size_t available = stream->available();
        if (available) {
          size_t toRead = min(available, size - bytesRead);
          size_t read = stream->readBytes(buffer + bytesRead, toRead);
          bytesRead += read;
        }
        delay(1);
      }
      
      // Decode and display JPEG
      if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
        // Update original image dimensions and size
        originalImageWidth = jpeg.getWidth();
        originalImageHeight = jpeg.getHeight();
        currentImageSize = bytesRead;
        
        // Check if image fits in our full image buffer
        if (originalImageWidth * originalImageHeight * 2 <= fullImageBufferSize) {
          // Set up full image buffer dimensions
          fullImageWidth = originalImageWidth;
          fullImageHeight = originalImageHeight;
          
          // Clear the full image buffer
          memset(fullImageBuffer, 0, fullImageWidth * fullImageHeight * sizeof(uint16_t));
          
          // Decode the full image into our buffer
          jpeg.decode(0, 0, 0);
          jpeg.close();
          
          // Now render the full image with smooth transition (single clean update)
          renderFullImageWithTransition(true);
        } else {
          jpeg.close();
        }
      }
    }
  }
  
  http.end();
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

void resetImageTransform() {
  scaleX = 1.0;
  scaleY = 1.0;
  offsetX = 0;
  offsetY = 0;
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
        
      // Reset command
      case 'R':
      case 'r':
        resetImageTransform();
        Serial.println("Reset transformations");
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
        Serial.println("Reset:");
        Serial.println("  R   : Reset all transformations");
        Serial.println("Help:");
        Serial.println("  H/? : Show this help");
        Serial.printf("Current: Scale %.1fx%.1f, Offset %d,%d\n", scaleX, scaleY, offsetX, offsetY);
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

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiConnected = false;
      if (!firstImageLoaded) {
        debugPrint("ERROR: WiFi disconnected!", RED);
        debugPrint("Attempting reconnection...", YELLOW);
      }
      connectToWiFi();
    }
    return;
  } else if (!wifiConnected) {
    wifiConnected = true;
    if (!firstImageLoaded) {
      debugPrint("WiFi reconnected!", GREEN);
    }
  }
  
  // Handle MQTT connection and messages
  handleMQTT();
  
  // Process serial commands for image control
  processSerialCommands();
  
  // Check if it's time to update the image
  unsigned long currentTime = millis();
  if (currentTime - lastUpdate >= updateInterval || lastUpdate == 0) {
    
    if (!firstImageLoaded) {
      // First download with debug output
      downloadAndDisplayImage();
    } else {
      // Subsequent downloads without debug output
      downloadAndDisplayImageSilent();
    }
    
    lastUpdate = currentTime;
  }
  
  delay(100);
}
