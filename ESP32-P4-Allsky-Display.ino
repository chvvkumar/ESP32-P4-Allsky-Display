#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include "displays_config.h"

// WiFi Configuration - Update these with your credentials
const char* ssid = "IoT";
const char* password = "kkkkkkkk";

// Image Configuration
const char* imageURL = "https://allsky.challa.co:1982/current/resized/image.jpg"; // Random image service
const unsigned long updateInterval = 10000; // 10 seconds in milliseconds

// Display variables
int16_t w, h;
unsigned long lastUpdate = 0;
bool wifiConnected = false;
uint8_t* imageBuffer = nullptr;
size_t imageBufferSize = 0;
bool firstImageLoaded = false; // Track if first image has been loaded

// Image transformation variables
float scaleX = 1.0;  // Horizontal scale factor
float scaleY = 1.0;  // Vertical scale factor
int16_t offsetX = 0; // Horizontal offset from center
int16_t offsetY = 0; // Vertical offset from center
int16_t originalImageWidth = 0;
int16_t originalImageHeight = 0;
size_t currentImageSize = 0; // Store the actual size of the current image data

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

// JPEG callback function to draw pixels with scaling and offset
int JPEGDraw(JPEGDRAW *pDraw) {
  // Apply scaling and offset transformations
  int16_t scaledX = (int16_t)(pDraw->x * scaleX);
  int16_t scaledY = (int16_t)(pDraw->y * scaleY);
  int16_t scaledWidth = (int16_t)(pDraw->iWidth * scaleX);
  int16_t scaledHeight = (int16_t)(pDraw->iHeight * scaleY);
  
  // Calculate center position
  int16_t centerX = w / 2;
  int16_t centerY = h / 2;
  
  // Calculate scaled image center
  int16_t scaledImageCenterX = (int16_t)(originalImageWidth * scaleX) / 2;
  int16_t scaledImageCenterY = (int16_t)(originalImageHeight * scaleY) / 2;
  
  // Apply centering and offset
  int16_t finalX = centerX - scaledImageCenterX + scaledX + offsetX;
  int16_t finalY = centerY - scaledImageCenterY + scaledY + offsetY;
  
  // Check bounds to avoid drawing outside screen
  if (finalX >= w || finalY >= h || finalX + scaledWidth <= 0 || finalY + scaledHeight <= 0) {
    return 1; // Skip drawing if completely outside screen
  }
  
  // For simple scaling, we'll use the built-in scaling if available, otherwise draw normally
  if (scaleX == 1.0 && scaleY == 1.0) {
    // No scaling, just apply offset
    gfx->draw16bitRGBBitmap(finalX, finalY, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  } else {
    // For now, draw at calculated position (basic implementation)
    // Note: True scaling would require pixel interpolation
    gfx->draw16bitRGBBitmap(finalX, finalY, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  }
  
  return 1;
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
  
  debugPrint("Memory allocated successfully", GREEN);
  debugPrintf(WHITE, "Free heap: %d bytes", ESP.getFreeHeap());
  debugPrintf(WHITE, "Free PSRAM: %d bytes", ESP.getFreePsram());
  
  // Connect to WiFi
  debugPrint("Starting WiFi connection...", YELLOW);
  connectToWiFi();
  
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
        debugPrint("Displaying image...", GREEN);
        
        // Clear screen and decode image with transformations
        gfx->fillScreen(BLACK);
        jpeg.decode(0, 0, 0);  // Start from 0,0 since positioning is handled in callback
        jpeg.close();
        
        // Mark first image as loaded
        firstImageLoaded = true;
        
        // Show status info at bottom
        gfx->setTextSize(1);
        gfx->setTextColor(WHITE);
        gfx->fillRect(0, h-20, w, 20, BLACK);
        gfx->setCursor(5, h-15);
        gfx->printf("Updated: %02d:%02d:%02d", 
                   (millis()/3600000)%24, 
                   (millis()/60000)%60, 
                   (millis()/1000)%60);
        
        debugPrint("SUCCESS: Image displayed!", GREEN);
        Serial.println("First image loaded successfully - switching to image mode");
        
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
  
  // Show loading message briefly
  gfx->fillRect(0, h-40, w, 20, BLACK);
  gfx->setTextSize(1);
  gfx->setTextColor(YELLOW);
  gfx->setCursor(5, h-35);
  gfx->print("Updating...");
  
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
        
        gfx->fillScreen(BLACK);
        jpeg.decode(0, 0, 0);  // Start from 0,0 since positioning is handled in callback
        jpeg.close();
        
        // Update timestamp
        gfx->setTextSize(1);
        gfx->setTextColor(WHITE);
        gfx->fillRect(0, h-20, w, 20, BLACK);
        gfx->setCursor(5, h-15);
        gfx->printf("Updated: %02d:%02d:%02d", 
                   (millis()/3600000)%24, 
                   (millis()/60000)%60, 
                   (millis()/1000)%60);
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
  if (!firstImageLoaded || !imageBuffer || currentImageSize == 0) return;
  
  // Redecode and display the current image with new transformations
  if (jpeg.openRAM(imageBuffer, currentImageSize, JPEGDraw)) {
    gfx->fillScreen(BLACK);
    jpeg.decode(0, 0, 0);
    jpeg.close();
    
    // Update status display
    updateStatusDisplay();
  }
}

void updateStatusDisplay() {
  // Show current transformation values and timestamp
  gfx->setTextSize(1);
  gfx->setTextColor(WHITE);
  gfx->fillRect(0, h-40, w, 40, BLACK);
  
  // Timestamp
  gfx->setCursor(5, h-35);
  gfx->printf("Updated: %02d:%02d:%02d", 
             (millis()/3600000)%24, 
             (millis()/60000)%60, 
             (millis()/1000)%60);
  
  // Transformation info
  gfx->setCursor(5, h-20);
  gfx->printf("Scale: %.1fx%.1f  Offset: %d,%d", scaleX, scaleY, offsetX, offsetY);
  
  // Controls info
  gfx->setCursor(5, h-5);
  gfx->setTextColor(CYAN);
  gfx->print("Controls: +/-XY scale, WASD move, R reset");
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
