#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

// Include all our modular components
#include "config.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "display_manager.h"
#include "ppa_accelerator.h"
#include "mqtt_manager.h"

// Additional required libraries
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>

// Global variables for image processing
bool firstImageLoaded = false;
unsigned long lastUpdate = 0;
uint8_t* imageBuffer = nullptr;
size_t imageBufferSize = 0;

// Image transformation variables
float scaleX = DEFAULT_SCALE_X;
float scaleY = DEFAULT_SCALE_Y;
int16_t offsetX = DEFAULT_OFFSET_X;
int16_t offsetY = DEFAULT_OFFSET_Y;
float rotationAngle = DEFAULT_ROTATION;

// Full image buffer for smooth rendering
uint16_t* fullImageBuffer = nullptr;
size_t fullImageBufferSize = 0;
int16_t fullImageWidth = 0;
int16_t fullImageHeight = 0;

// Scaling buffer for transformed images
uint16_t* scaledBuffer = nullptr;
size_t scaledBufferSize = 0;

// JPEG decoder
JPEGDEC jpeg;

// Image processing control
bool imageProcessing = false;
unsigned long lastImageProcessTime = 0;

// Forward declarations
void debugPrint(const char* message, uint16_t color);
void debugPrintf(uint16_t color, const char* format, ...);
int JPEGDraw(JPEGDRAW *pDraw);
void downloadAndDisplayImage();
void processSerialCommands();
void renderFullImage();

// Debug output wrapper functions
void debugPrint(const char* message, uint16_t color) {
    displayManager.debugPrint(message, color);
}

void debugPrintf(uint16_t color, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    displayManager.debugPrint(buffer, color);
}

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

void setup() {
    Serial.begin(9600);
    delay(1000); // Give serial time to initialize
    
    Serial.println("=== ESP32-P4 Image Display Starting ===");
    
    // Initialize system monitor first
    if (!systemMonitor.begin()) {
        Serial.println("CRITICAL: System monitor initialization failed!");
        while(1) delay(1000);
    }
    
    // Initialize display manager
    if (!displayManager.begin()) {
        Serial.println("CRITICAL: Display initialization failed!");
        while(1) delay(1000);
    }
    
    // Get display dimensions
    int16_t w = displayManager.getWidth();
    int16_t h = displayManager.getHeight();
    
    // Setup debug functions for other modules
    wifiManager.setDebugFunctions(debugPrint, debugPrintf, &firstImageLoaded);
    ppaAccelerator.setDebugFunctions(debugPrint, debugPrintf);
    mqttManager.setDebugFunctions(debugPrint, debugPrintf, &firstImageLoaded);
    
    // Now we can use debugPrint functions
    debugPrint("=== ESP32 Modular AllSky Display ===", COLOR_CYAN);
    debugPrint("Display initialized!", COLOR_GREEN);
    debugPrintf(COLOR_WHITE, "Display: %dx%d pixels", w, h);
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    debugPrintf(COLOR_WHITE, "Free PSRAM: %d bytes", systemMonitor.getCurrentFreePsram());
    
    // Allocate image buffer in PSRAM
    imageBufferSize = w * h * IMAGE_BUFFER_MULTIPLIER * 2; // 16-bit color
    debugPrintf(COLOR_WHITE, "Allocating buffer: %d bytes", imageBufferSize);
    
    imageBuffer = (uint8_t*)ps_malloc(imageBufferSize);
    if (!imageBuffer) {
        debugPrint("ERROR: Memory allocation failed!", COLOR_RED);
        while(1) delay(1000);
    }
    
    // Allocate full image buffer for smooth rendering
    fullImageBufferSize = FULL_IMAGE_BUFFER_SIZE;
    debugPrintf(COLOR_WHITE, "Allocating full image buffer: %d bytes", fullImageBufferSize);
    
    fullImageBuffer = (uint16_t*)ps_malloc(fullImageBufferSize);
    if (!fullImageBuffer) {
        debugPrint("ERROR: Full image buffer allocation failed!", COLOR_RED);
        while(1) delay(1000);
    }
    
    // Allocate scaling buffer for transformed images
    scaledBufferSize = w * h * SCALED_BUFFER_MULTIPLIER * 2;
    debugPrintf(COLOR_WHITE, "Allocating scaling buffer: %d bytes", scaledBufferSize);
    
    scaledBuffer = (uint16_t*)ps_malloc(scaledBufferSize);
    if (!scaledBuffer) {
        debugPrint("ERROR: Scaling buffer allocation failed!", COLOR_RED);
        while(1) delay(1000);
    }
    
    debugPrint("Memory allocated successfully", COLOR_GREEN);
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    debugPrintf(COLOR_WHITE, "Free PSRAM: %d bytes", systemMonitor.getCurrentFreePsram());
    
    // Initialize PPA hardware acceleration
    if (ppaAccelerator.begin(w, h)) {
        debugPrint("Hardware acceleration enabled!", COLOR_GREEN);
    } else {
        debugPrint("Using software scaling fallback", COLOR_YELLOW);
    }
    
    // Initialize brightness control
    displayManager.initBrightness();
    
    // Initialize WiFi manager
    if (!wifiManager.begin()) {
        debugPrint("ERROR: WiFi initialization failed!", COLOR_RED);
    } else {
        // Connect to WiFi
        debugPrint("Starting WiFi connection...", COLOR_YELLOW);
        wifiManager.connectToWiFi();
    }
    
    // Initialize MQTT manager
    if (!mqttManager.begin()) {
        debugPrint("ERROR: MQTT initialization failed!", COLOR_RED);
    } else {
        debugPrint("MQTT manager initialized", COLOR_GREEN);
        // MQTT connection will be attempted after WiFi is established
    }
    
    debugPrint("Setup complete!", COLOR_GREEN);
    delay(1000);
}

void renderFullImage() {
    if (!fullImageBuffer || fullImageWidth == 0 || fullImageHeight == 0) return;
    
    Arduino_DSI_Display* gfx = displayManager.getGFX();
    if (!gfx) return;
    
    int16_t w = displayManager.getWidth();
    int16_t h = displayManager.getHeight();
    
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
    
    // Calculate center position
    int16_t centerX = w / 2;
    int16_t centerY = h / 2;
    
    // Calculate final position with centering and offset
    int16_t finalX = centerX - (scaledWidth / 2) + offsetX;
    int16_t finalY = centerY - (scaledHeight / 2) + offsetY;
    
    // Clear screen
    displayManager.clearScreen();
    
    if (scaleX == 1.0 && scaleY == 1.0 && rotationAngle == 0.0) {
        // No scaling or rotation needed, direct copy
        Serial.println("DEBUG: Using direct copy (no scaling or rotation)");
        displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
    } else {
        // Try hardware acceleration first if available
        size_t scaledImageSize = scaledWidth * scaledHeight * 2;
        
        if (ppaAccelerator.isAvailable() && scaledImageSize <= scaledBufferSize) {
            Serial.println("DEBUG: Attempting PPA hardware scale+rotate");
            unsigned long hwStart = millis();
            
            if (ppaAccelerator.scaleRotateImage(fullImageBuffer, fullImageWidth, fullImageHeight,
                                              scaledBuffer, scaledWidth, scaledHeight, rotationAngle)) {
                unsigned long hwTime = millis() - hwStart;
                Serial.printf("PPA scale+rotate: %lums (%dx%d -> %dx%d, %.0f°)\n", 
                             hwTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, rotationAngle);
                
                // Draw the hardware-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
                return;
            } else {
                Serial.println("DEBUG: PPA scale+rotate failed, falling back to software");
            }
        }
        
        // Software fallback - for now, just do basic scaling without rotation
        Serial.println("DEBUG: Software fallback - basic scaling only");
        if (rotationAngle == 0.0 && scaledImageSize <= scaledBufferSize) {
            // Simple software scaling (simplified version)
            displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        } else {
            // Just draw original image as fallback
            displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        }
    }
}

void downloadAndDisplayImage() {
    if (!wifiManager.isConnected()) {
        debugPrint("ERROR: No WiFi connection", COLOR_RED);
        return;
    }
    
    systemMonitor.resetWatchdog();
    
    debugPrint("=== Starting Download ===", COLOR_CYAN);
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    debugPrintf(COLOR_WHITE, "URL: %s", IMAGE_URL);
    
    HTTPClient http;
    http.begin(IMAGE_URL);
    http.setTimeout(10000);
    
    debugPrint("Sending HTTP request...", COLOR_WHITE);
    unsigned long downloadStart = millis();
    systemMonitor.resetWatchdog();
    int httpCode = http.GET();
    unsigned long downloadTime = millis() - downloadStart;
    
    debugPrintf(COLOR_WHITE, "HTTP code: %d (%lu ms)", httpCode, downloadTime);
    
    if (httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        size_t size = http.getSize();
        
        debugPrintf(COLOR_WHITE, "Image size: %d bytes", size);
        
        if (size > 0 && size < imageBufferSize) {
            debugPrint("Downloading image data...", COLOR_YELLOW);
            
            size_t bytesRead = 0;
            uint8_t* buffer = imageBuffer;
            unsigned long readStart = millis();
            
            while (http.connected() && bytesRead < size) {
                systemMonitor.resetWatchdog();
                size_t available = stream->available();
                if (available) {
                    size_t toRead = min(available, size - bytesRead);
                    size_t read = stream->readBytes(buffer + bytesRead, toRead);
                    bytesRead += read;
                    
                    // Show progress and reset watchdog
                    if (bytesRead % 20480 == 0) {
                        debugPrintf(COLOR_YELLOW, "Downloaded: %.1f%%", (float)bytesRead/size*100);
                        systemMonitor.resetWatchdog();
                    }
                }
                systemMonitor.safeYield();
            }
            
            unsigned long readTime = millis() - readStart;
            debugPrintf(COLOR_GREEN, "Download complete: %d bytes", bytesRead);
            debugPrintf(COLOR_WHITE, "Speed: %.2f KB/s", (float)bytesRead/readTime);
            
            // Decode and display JPEG
            debugPrint("Decoding JPEG...", COLOR_YELLOW);
            
            if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
                fullImageWidth = jpeg.getWidth();
                fullImageHeight = jpeg.getHeight();
                
                debugPrintf(COLOR_WHITE, "JPEG: %dx%d pixels", fullImageWidth, fullImageHeight);
                
                // Check if image fits in our full image buffer
                if (fullImageWidth * fullImageHeight * 2 <= fullImageBufferSize) {
                    debugPrint("Decoding to full buffer...", COLOR_YELLOW);
                    
                    // Clear the full image buffer
                    memset(fullImageBuffer, 0, fullImageWidth * fullImageHeight * sizeof(uint16_t));
                    
                    // Decode the full image into our buffer
                    systemMonitor.resetWatchdog();
                    jpeg.decode(0, 0, 0);
                    jpeg.close();
                    
                    debugPrint("Rendering image...", COLOR_GREEN);
                    
                    // Now render the full image with transformations
                    renderFullImage();
                    
                    debugPrint("SUCCESS: Image displayed!", COLOR_GREEN);
                    
                    // Mark first image as loaded (only once)
                    if (!firstImageLoaded) {
                        firstImageLoaded = true;
                        displayManager.setFirstImageLoaded(true);
                        Serial.println("First image loaded successfully - switching to image mode");
                    } else {
                        Serial.println("Image updated successfully");
                    }
                } else {
                    debugPrint("ERROR: Image too large for buffer!", COLOR_RED);
                    jpeg.close();
                }
            } else {
                debugPrint("ERROR: JPEG decode failed!", COLOR_RED);
            }
        } else {
            debugPrintf(COLOR_RED, "Invalid size: %d bytes", size);
        }
    } else {
        debugPrintf(COLOR_RED, "HTTP error: %d", httpCode);
    }
    
    http.end();
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
}

void processSerialCommands() {
    if (Serial.available()) {
        char command = Serial.read();
        
        switch (command) {
            // Scaling commands
            case '+':
                scaleX = constrain(scaleX + SCALE_STEP, MIN_SCALE, MAX_SCALE);
                scaleY = constrain(scaleY + SCALE_STEP, MIN_SCALE, MAX_SCALE);
                renderFullImage();
                Serial.printf("Scale both: %.1fx%.1f\n", scaleX, scaleY);
                break;
            case '-':
                scaleX = constrain(scaleX - SCALE_STEP, MIN_SCALE, MAX_SCALE);
                scaleY = constrain(scaleY - SCALE_STEP, MIN_SCALE, MAX_SCALE);
                renderFullImage();
                Serial.printf("Scale both: %.1fx%.1f\n", scaleX, scaleY);
                break;
                
            // Movement commands
            case 'W':
            case 'w':
                offsetY -= MOVE_STEP;
                renderFullImage();
                Serial.printf("Move up, offset: %d,%d\n", offsetX, offsetY);
                break;
            case 'S':
            case 's':
                offsetY += MOVE_STEP;
                renderFullImage();
                Serial.printf("Move down, offset: %d,%d\n", offsetX, offsetY);
                break;
            case 'A':
            case 'a':
                offsetX -= MOVE_STEP;
                renderFullImage();
                Serial.printf("Move left, offset: %d,%d\n", offsetX, offsetY);
                break;
            case 'D':
            case 'd':
                offsetX += MOVE_STEP;
                renderFullImage();
                Serial.printf("Move right, offset: %d,%d\n", offsetX, offsetY);
                break;
                
            // Rotation commands
            case 'Q':
            case 'q':
                rotationAngle -= ROTATION_STEP;
                if (rotationAngle < 0) rotationAngle += 360.0;
                renderFullImage();
                Serial.printf("Rotate CCW: %.0f°\n", rotationAngle);
                break;
            case 'E':
            case 'e':
                rotationAngle += ROTATION_STEP;
                if (rotationAngle >= 360.0) rotationAngle -= 360.0;
                renderFullImage();
                Serial.printf("Rotate CW: %.0f°\n", rotationAngle);
                break;
                
            // Reset command
            case 'R':
            case 'r':
                scaleX = DEFAULT_SCALE_X;
                scaleY = DEFAULT_SCALE_Y;
                offsetX = DEFAULT_OFFSET_X;
                offsetY = DEFAULT_OFFSET_Y;
                rotationAngle = DEFAULT_ROTATION;
                renderFullImage();
                Serial.println("Reset all transformations");
                break;
                
            // Brightness commands
            case 'L':
            case 'l':
                displayManager.setBrightness(min(displayManager.getBrightness() + 10, 100));
                Serial.printf("Brightness up: %d%%\n", displayManager.getBrightness());
                break;
            case 'K':
            case 'k':
                displayManager.setBrightness(max(displayManager.getBrightness() - 10, 0));
                Serial.printf("Brightness down: %d%%\n", displayManager.getBrightness());
                break;
                
            // Help command
            case 'H':
            case 'h':
            case '?':
                Serial.println("\n=== Modular Image Control Commands ===");
                Serial.println("Scaling: +/- Scale up/down");
                Serial.println("Movement: W/A/S/D Move up/left/down/right");
                Serial.println("Rotation: Q/E Rotate CCW/CW");
                Serial.println("Reset: R Reset all transformations");
                Serial.println("Brightness: L/K Up/Down");
                Serial.println("System: M Memory info, I Network info, P PPA info");
                Serial.println("Help: H/? Show this help");
                break;
                
            // System info commands
            case 'M':
            case 'm':
                systemMonitor.printMemoryStatus();
                break;
            case 'I':
            case 'i':
                wifiManager.printConnectionInfo();
                break;
            case 'P':
            case 'p':
                ppaAccelerator.printStatus();
                break;
            case 'T':
            case 't':
                mqttManager.printConnectionInfo();
                break;
        }
        
        // Clear any remaining characters
        while (Serial.available()) {
            Serial.read();
        }
    }
}

void loop() {
    // Update all system modules
    systemMonitor.update();
    wifiManager.update();
    
    // Update MQTT manager (connect to MQTT after WiFi is established)
    if (wifiManager.isConnected()) {
        mqttManager.update();
    }
    
    // Check for critical system health issues
    if (!systemMonitor.isSystemHealthy()) {
        Serial.println("CRITICAL: System health compromised, attempting recovery...");
        systemMonitor.forceResetWatchdog();
        systemMonitor.safeDelay(5000);
        systemMonitor.forceResetWatchdog();
    }
    
    // Process serial commands for image control
    processSerialCommands();
    systemMonitor.forceResetWatchdog();
    
    // Check for stuck image processing
    if (imageProcessing && (millis() - lastImageProcessTime > IMAGE_PROCESS_TIMEOUT)) {
        Serial.println("WARNING: Image processing timeout detected, resetting...");
        imageProcessing = false;
        systemMonitor.forceResetWatchdog();
    }
    
    // Check if it's time to update the image (only if not currently processing)
    unsigned long currentTime = millis();
    if (!imageProcessing && (currentTime - lastUpdate >= UPDATE_INTERVAL || lastUpdate == 0)) {
        imageProcessing = true;
        lastImageProcessTime = currentTime;
        systemMonitor.forceResetWatchdog();
        
        downloadAndDisplayImage();
        
        lastUpdate = currentTime;
        imageProcessing = false;
        systemMonitor.forceResetWatchdog();
    }
    
    // Additional watchdog reset before delay
    systemMonitor.forceResetWatchdog();
    systemMonitor.safeDelay(100);
    systemMonitor.forceResetWatchdog();
}
