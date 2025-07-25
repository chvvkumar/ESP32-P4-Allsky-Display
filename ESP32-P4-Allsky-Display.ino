#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

// Include all our modular components
#include "config.h"
#include "config_storage.h"
#include "web_config.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "display_manager.h"
#include "ppa_accelerator.h"
#include "mqtt_manager.h"
#include "gt911.h"
#include "i2c.h"

// Additional required libraries
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>

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

// Dynamic configuration variables
unsigned long currentUpdateInterval = UPDATE_INTERVAL;

// Multi-image cycling variables
unsigned long lastCycleTime = 0;
unsigned long currentCycleInterval = DEFAULT_CYCLE_INTERVAL;
bool cyclingEnabled = false;
bool randomOrderEnabled = false;
int currentImageIndex = 0;
int imageSourceCount = 1;
String currentImageURL = "";

// Full image buffer for smooth rendering
uint16_t* fullImageBuffer = nullptr;
size_t fullImageBufferSize = 0;
int16_t fullImageWidth = 0;
int16_t fullImageHeight = 0;

// Scaling buffer for transformed images
uint16_t* scaledBuffer = nullptr;
size_t scaledBufferSize = 0;

// Previous image tracking for seamless transitions
static int16_t prevImageX = -1, prevImageY = -1;
static int16_t prevImageWidth = 0, prevImageHeight = 0;
static bool hasSeenFirstImage = false;

// JPEG decoder
JPEGDEC jpeg;

// Image processing control
bool imageProcessing = false;
unsigned long lastImageProcessTime = 0;

// Touch control variables
esp_lcd_touch_handle_t touchHandle = nullptr;
bool touchEnabled = false;

// Touch gesture detection variables
enum TouchState {
    TOUCH_IDLE,
    TOUCH_PRESSED,
    TOUCH_RELEASED,
    TOUCH_WAITING_FOR_SECOND_TAP
};

TouchState touchState = TOUCH_IDLE;
unsigned long touchPressTime = 0;
unsigned long touchReleaseTime = 0;
unsigned long firstTapTime = 0;
bool touchPressed = false;
bool touchWasPressed = false;

// Touch timing configuration
const unsigned long TOUCH_DEBOUNCE_MS = 50;        // Minimum time between touch events
const unsigned long DOUBLE_TAP_TIMEOUT_MS = 400;   // Maximum time between taps for double-tap
const unsigned long MIN_TAP_DURATION_MS = 50;      // Minimum press duration for valid tap
const unsigned long MAX_TAP_DURATION_MS = 2000;    // Maximum press duration for tap (vs hold)

// Touch actions triggered flags
bool touchTriggeredNextImage = false;
bool touchTriggeredModeToggle = false;

// Touch mode control
bool singleImageRefreshMode = false;  // false = cycling mode, true = single image refresh mode

// Forward declarations
void debugPrint(const char* message, uint16_t color);
void debugPrintf(uint16_t color, const char* format, ...);
int JPEGDraw(JPEGDRAW *pDraw);
void downloadAndDisplayImage();
void processSerialCommands();
void renderFullImage();
void loadCyclingConfiguration();
void advanceToNextImage();
String getCurrentImageURL();
void updateCyclingVariables();

// Touch function declarations
void initializeTouchController();
void updateTouchState();
void processTouchGestures();
void handleSingleTap();
void handleDoubleTap();

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
    
    // Initialize configuration system first
    Serial.println("Initializing configuration...");
    initializeConfiguration();
    
    // Initialize system monitor
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
    
    // Start web configuration server if WiFi is connected
    if (wifiManager.isConnected()) {
        debugPrint("Starting web configuration server...", COLOR_YELLOW);
        if (webConfig.begin(8080)) {
            debugPrintf(COLOR_GREEN, "Web config available at: http://%s:8080", WiFi.localIP().toString().c_str());
            Serial.printf("Web configuration server started successfully at: http://%s:8080\n", WiFi.localIP().toString().c_str());
        } else {
            debugPrint("ERROR: Web config server failed to start", COLOR_RED);
        }
    } else {
        debugPrint("WiFi not connected - web server will start after connection", COLOR_YELLOW);
    }
    
    // Load cycling configuration after all modules are initialized
    loadCyclingConfiguration();
    
    // Initialize touch controller
    initializeTouchController();
    
    debugPrint("Setup complete!", COLOR_GREEN);
    delay(1000);
}

void renderFullImage() {
    // Reset watchdog at function start
    systemMonitor.forceResetWatchdog();
    
    if (!fullImageBuffer || fullImageWidth == 0 || fullImageHeight == 0) {
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    Arduino_DSI_Display* gfx = displayManager.getGFX();
    if (!gfx) {
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    int16_t w = displayManager.getWidth();
    int16_t h = displayManager.getHeight();
    
    // Reset watchdog before calculations
    systemMonitor.forceResetWatchdog();
    
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
    
    // Smart clearing to prevent flash
    systemMonitor.forceResetWatchdog();
    
    if (!hasSeenFirstImage) {
        // First image: clear entire screen
        debugPrint("DEBUG: First image - clearing entire screen", COLOR_WHITE);
        displayManager.clearScreen();
        hasSeenFirstImage = true;
        systemMonitor.forceResetWatchdog();
    } else {
        // Subsequent images: clear only areas not covered by new image
        debugPrintf(COLOR_WHITE, "DEBUG: Smart clear - prev(%d,%d %dx%d) -> new(%d,%d %dx%d)", 
                   prevImageX, prevImageY, prevImageWidth, prevImageHeight,
                   finalX, finalY, scaledWidth, scaledHeight);
        
        // Reset watchdog before clearing operations
        systemMonitor.forceResetWatchdog();
        
        // Clear areas that were covered by previous image but won't be covered by new image
        if (prevImageWidth > 0 && prevImageHeight > 0) {
            // Clear top area (above new image)
            if (prevImageY < finalY) {
                int16_t clearX = min(prevImageX, finalX);
                int16_t clearWidth = max(prevImageX + prevImageWidth, finalX + scaledWidth) - clearX;
                gfx->fillRect(clearX, prevImageY, clearWidth, finalY - prevImageY, COLOR_BLACK);
            }
            
            // Clear bottom area (below new image)
            int16_t prevBottom = prevImageY + prevImageHeight;
            int16_t newBottom = finalY + scaledHeight;
            if (prevBottom > newBottom) {
                int16_t clearX = min(prevImageX, finalX);
                int16_t clearWidth = max(prevImageX + prevImageWidth, finalX + scaledWidth) - clearX;
                gfx->fillRect(clearX, newBottom, clearWidth, prevBottom - newBottom, COLOR_BLACK);
            }
            
            // Clear left area (left of new image)
            if (prevImageX < finalX) {
                int16_t clearY = max(prevImageY, finalY);
                int16_t clearHeight = min(prevImageY + prevImageHeight, finalY + scaledHeight) - clearY;
                if (clearHeight > 0) {
                    gfx->fillRect(prevImageX, clearY, finalX - prevImageX, clearHeight, COLOR_BLACK);
                }
            }
            
            // Clear right area (right of new image)
            int16_t prevRight = prevImageX + prevImageWidth;
            int16_t newRight = finalX + scaledWidth;
            if (prevRight > newRight) {
                int16_t clearY = max(prevImageY, finalY);
                int16_t clearHeight = min(prevImageY + prevImageHeight, finalY + scaledHeight) - clearY;
                if (clearHeight > 0) {
                    gfx->fillRect(newRight, clearY, prevRight - newRight, clearHeight, COLOR_BLACK);
                }
            }
        }
    }
    
    // Reset watchdog before rendering operations
    systemMonitor.forceResetWatchdog();
    
    if (scaleX == 1.0 && scaleY == 1.0 && rotationAngle == 0.0) {
        // No scaling or rotation needed, direct copy
        debugPrint("DEBUG: Using direct copy (no scaling or rotation)", COLOR_WHITE);
        displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        
        systemMonitor.forceResetWatchdog();
        
        // Update tracking variables for next transition
        prevImageX = finalX;
        prevImageY = finalY;
        prevImageWidth = fullImageWidth;
        prevImageHeight = fullImageHeight;
    } else {
        // Try hardware acceleration first if available
        size_t scaledImageSize = scaledWidth * scaledHeight * 2;
        
        if (ppaAccelerator.isAvailable() && scaledImageSize <= scaledBufferSize) {
            debugPrint("DEBUG: Attempting PPA hardware scale+rotate", COLOR_WHITE);
            systemMonitor.forceResetWatchdog();
            
            unsigned long hwStart = millis();
            
            if (ppaAccelerator.scaleRotateImage(fullImageBuffer, fullImageWidth, fullImageHeight,
                                              scaledBuffer, scaledWidth, scaledHeight, rotationAngle)) {
                unsigned long hwTime = millis() - hwStart;
                debugPrintf(COLOR_WHITE, "PPA scale+rotate: %lums (%dx%d -> %dx%d, %.0f°)", 
                           hwTime, fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, rotationAngle);
                
                systemMonitor.forceResetWatchdog();
                
                // Draw the hardware-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
                
                systemMonitor.forceResetWatchdog();
                
                // Update tracking variables for next transition
                prevImageX = finalX;
                prevImageY = finalY;
                prevImageWidth = scaledWidth;
                prevImageHeight = scaledHeight;
                return;
            } else {
                debugPrint("DEBUG: PPA scale+rotate failed, falling back to software", COLOR_YELLOW);
                systemMonitor.forceResetWatchdog();
            }
        }
        
        // Software fallback - for now, just do basic scaling without rotation
        debugPrint("DEBUG: Software fallback - basic scaling only", COLOR_YELLOW);
        systemMonitor.forceResetWatchdog();
        
        if (rotationAngle == 0.0 && scaledImageSize <= scaledBufferSize) {
            // Simple software scaling (simplified version)
            displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
            
            systemMonitor.forceResetWatchdog();
            
            // Update tracking variables for next transition
            prevImageX = finalX;
            prevImageY = finalY;
            prevImageWidth = fullImageWidth;
            prevImageHeight = fullImageHeight;
        } else {
            // Just draw original image as fallback
            displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
            
            systemMonitor.forceResetWatchdog();
            
            // Update tracking variables for next transition
            prevImageX = finalX;
            prevImageY = finalY;
            prevImageWidth = fullImageWidth;
            prevImageHeight = fullImageHeight;
        }
    }
    
    // Final watchdog reset before function exit
    systemMonitor.forceResetWatchdog();
}

void downloadAndDisplayImage() {
    // Immediate debug output with Serial.println to ensure it shows up
    Serial.println("=== DOWNLOADANDDISPLAYIMAGE FUNCTION START ===");
    Serial.flush();
    
    // Reset watchdog at function start
    systemMonitor.forceResetWatchdog();
    
    Serial.println("DEBUG: Watchdog reset complete");
    Serial.flush();
    
    // Enhanced network connectivity check
    if (!wifiManager.isConnected()) {
        Serial.println("ERROR: No WiFi connection");
        Serial.flush();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    // Additional network health check - ping gateway or DNS
    Serial.println("DEBUG: Performing network health check...");
    Serial.flush();
    
    // Test network connectivity with a simple ping-like operation
    WiFiClient testClient;
    testClient.setTimeout(NETWORK_CHECK_TIMEOUT);
    
    bool networkHealthy = false;
    unsigned long networkTestStart = millis();
    
    // Try to connect to a reliable server to test network health
    if (testClient.connect("8.8.8.8", 53)) {  // Google DNS
        testClient.stop();
        networkHealthy = true;
        Serial.println("DEBUG: Network health check passed");
    } else {
        Serial.printf("DEBUG: Network health check failed after %lu ms\n", millis() - networkTestStart);
    }
    
    systemMonitor.forceResetWatchdog();
    
    if (!networkHealthy) {
        Serial.println("WARNING: Network appears unhealthy, proceeding with caution");
        debugPrint("WARNING: Network connectivity issues detected", COLOR_YELLOW);
    }
    
    Serial.println("DEBUG: WiFi connection check passed");
    Serial.flush();
    
    // Get current image URL based on cycling configuration
    String imageURL = getCurrentImageURL();
    
    Serial.println("DEBUG: About to get current image URL");
    Serial.flush();
    Serial.printf("DEBUG: Current image URL: %s\n", imageURL.c_str());
    Serial.flush();
    
    debugPrint("=== Starting Download ===", COLOR_CYAN);
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    debugPrintf(COLOR_WHITE, "URL: %s", imageURL.c_str());
    
    Serial.println("DEBUG: Debug messages printed");
    Serial.flush();
    
    // Reset watchdog before HTTP operations
    systemMonitor.forceResetWatchdog();
    
    Serial.println("DEBUG: About to create HTTPClient");
    Serial.flush();
    
    HTTPClient http;
    
    Serial.println("DEBUG: HTTPClient created");
    Serial.flush();
    
    // Add timeout protection for HTTP begin
    debugPrint("Initializing HTTP client...", COLOR_WHITE);
    
    Serial.println("DEBUG: About to call http.begin()");
    Serial.flush();
    
    // Reset watchdog before potentially blocking http.begin() call
    systemMonitor.forceResetWatchdog();
    
    unsigned long httpBeginStart = millis();
    
    // Enhanced HTTP begin operation with task-based timeout protection
    bool httpBeginSuccess = false;
    
    // Start the HTTP begin operation with enhanced monitoring
    Serial.println("DEBUG: Starting http.begin() with enhanced timeout protection");
    Serial.flush();
    
    // Create a flag for completion monitoring
    volatile bool beginCompleted = false;
    volatile bool beginResult = false;
    
    // Reset watchdog right before critical operation
    systemMonitor.forceResetWatchdog();
    
    // Use a lambda function to wrap the http.begin call
    auto beginOperation = [&]() {
        beginResult = http.begin(imageURL);
        beginCompleted = true;
    };
    
    // Execute with timeout monitoring
    unsigned long beginStartTime = millis();
    beginOperation();
    
    unsigned long httpBeginTime = millis() - httpBeginStart;
    
    // Reset watchdog immediately after http.begin()
    systemMonitor.forceResetWatchdog();
    
    if (httpBeginTime > HTTP_BEGIN_TIMEOUT) {
        debugPrintf(COLOR_RED, "ERROR: HTTP begin took too long: %lu ms", httpBeginTime);
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    if (!beginResult) {
        debugPrintf(COLOR_RED, "ERROR: HTTP begin failed after %lu ms", httpBeginTime);
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    debugPrintf(COLOR_WHITE, "HTTP begin took: %lu ms", httpBeginTime);
    
    // Set enhanced timeouts to prevent any blocking
    http.setTimeout(HTTP_REQUEST_TIMEOUT);      // Use config value
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT); // Use config value
    
    // Add User-Agent and other headers for better compatibility
    http.addHeader("User-Agent", "ESP32-AllSky/1.0");
    http.addHeader("Connection", "close");
    http.addHeader("Cache-Control", "no-cache");
    
    // Reset watchdog before GET request
    systemMonitor.forceResetWatchdog();
    
    debugPrint("Sending HTTP request...", COLOR_WHITE);
    unsigned long downloadStart = millis();
    
    // Enhanced GET request with timeout monitoring
    int httpCode = -1;
    unsigned long getRequestStart = millis();
    
    // Reset watchdog right before GET with frequent monitoring
    systemMonitor.forceResetWatchdog();
    
    // Start GET request with enhanced error handling
    try {
        httpCode = http.GET();
    } catch (...) {
        debugPrint("ERROR: Exception during HTTP GET", COLOR_RED);
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    unsigned long getRequestTime = millis() - getRequestStart;
    
    // Immediate timeout check with config value
    if (getRequestTime >= HTTP_REQUEST_TIMEOUT) {
        debugPrintf(COLOR_RED, "ERROR: HTTP GET timed out after %lu ms", getRequestTime);
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    // Reset watchdog immediately after GET
    systemMonitor.forceResetWatchdog();
    
    debugPrintf(COLOR_WHITE, "HTTP code: %d (GET: %lu ms)", httpCode, getRequestTime);
    
    // Enhanced error handling for different HTTP codes
    if (httpCode != HTTP_CODE_OK) {
        if (httpCode == HTTP_CODE_NOT_FOUND) {
            debugPrint("ERROR: Image not found (404)", COLOR_RED);
        } else if (httpCode == HTTP_CODE_INTERNAL_SERVER_ERROR) {
            debugPrint("ERROR: Server error (500)", COLOR_RED);
        } else if (httpCode < 0) {
            debugPrintf(COLOR_RED, "ERROR: Connection error: %d", httpCode);
        } else {
            debugPrintf(COLOR_RED, "ERROR: HTTP error: %d", httpCode);
        }
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    // Reset watchdog before processing response
    systemMonitor.forceResetWatchdog();
    
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    
    debugPrintf(COLOR_WHITE, "Image size: %d bytes", size);
    
    // Validate size before proceeding
    if (size <= 0 || size >= imageBufferSize) {
        debugPrintf(COLOR_RED, "Invalid size: %d bytes", size);
        http.end();
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    debugPrint("Downloading image data...", COLOR_YELLOW);
    
    size_t bytesRead = 0;
    uint8_t* buffer = imageBuffer;
    unsigned long readStart = millis();
    
    // Use configuration values for better consistency
    const size_t ULTRA_CHUNK_SIZE = 1024;  // 1KB chunks for good performance
    const unsigned long DOWNLOAD_WATCHDOG_INTERVAL = 50;  // Reset every 50ms for more frequent resets
    const unsigned long NO_DATA_TIMEOUT = 2000;  // 2 seconds with no data before giving up
    
    unsigned long downloadStartTime = millis();
    unsigned long lastProgressTime = millis();
    unsigned long lastWatchdogReset = millis();
    unsigned long lastDataTime = millis();  // Track when we last received data
    
    while (http.connected() && bytesRead < size) {
        // Ultra-frequent watchdog reset - every 50ms
        if (millis() - lastWatchdogReset > DOWNLOAD_WATCHDOG_INTERVAL) {
            systemMonitor.forceResetWatchdog();
            lastWatchdogReset = millis();
        }
        
        // Check for overall download timeout
        if (millis() - downloadStartTime > TOTAL_DOWNLOAD_TIMEOUT) {
            debugPrint("ERROR: Download timeout - aborting", COLOR_RED);
            break;
        }
        
        // Check if connection is still alive
        if (!http.connected()) {
            debugPrint("ERROR: Connection lost during download", COLOR_RED);
            break;
        }
        
        // Check stream availability
        size_t available = stream->available();
        
        if (available > 0) {
            // Reset the last data time since we have data available
            lastDataTime = millis();
            
            // Calculate chunk size with safety limits
            size_t remainingBytes = size - bytesRead;
            size_t chunkSize = min(min(ULTRA_CHUNK_SIZE, available), remainingBytes);
            
            // Ensure we don't exceed buffer bounds
            if (bytesRead + chunkSize > imageBufferSize) {
                debugPrint("ERROR: Buffer overflow protection triggered", COLOR_RED);
                break;
            }
            
            // Read with timeout monitoring
            unsigned long chunkStart = millis();
            systemMonitor.forceResetWatchdog();
            
            size_t read = stream->readBytes(buffer + bytesRead, chunkSize);
            
            systemMonitor.forceResetWatchdog();
            unsigned long chunkTime = millis() - chunkStart;
            
            // Check for chunk timeout
            if (chunkTime > DOWNLOAD_CHUNK_TIMEOUT) {
                debugPrintf(COLOR_YELLOW, "WARNING: Chunk read took %lu ms", chunkTime);
                // If chunks are consistently slow, abort
                if (chunkTime > DOWNLOAD_CHUNK_TIMEOUT * 2) {  // More aggressive - 2x timeout
                    debugPrint("ERROR: Chunk timeout - aborting", COLOR_RED);
                    break;
                }
            }
            
            if (read > 0) {
                bytesRead += read;
                lastDataTime = millis();  // Update last data time on successful read
                
                // Show progress and reset watchdog
                if (millis() - lastProgressTime > 1000) {  // Every 1 second
                    debugPrintf(COLOR_YELLOW, "Downloaded: %.1f%% (%d/%d bytes)", 
                               (float)bytesRead/size*100, bytesRead, size);
                    lastProgressTime = millis();
                    systemMonitor.forceResetWatchdog();
                }
            } else {
                // No data read despite available data - brief delay and continue
                systemMonitor.forceResetWatchdog();
                delay(10);
            }
            
        } else {
            // No data available - check for stall using lastDataTime
            if (millis() - lastDataTime > NO_DATA_TIMEOUT) {
                debugPrintf(COLOR_RED, "ERROR: No data received for %lu ms", millis() - lastDataTime);
                break;
            }
            
            systemMonitor.forceResetWatchdog();
            delay(25);  // Shorter delay when no data available
        }
        
        // Force yield and watchdog reset
        yield();
        systemMonitor.forceResetWatchdog();
    }
    
    unsigned long readTime = millis() - readStart;
    debugPrintf(COLOR_GREEN, "Download complete: %d bytes in %lu ms", bytesRead, readTime);
    
    if (readTime > 0) {
        debugPrintf(COLOR_WHITE, "Speed: %.2f KB/s", (float)bytesRead/readTime);
    }
    
    // Close HTTP connection immediately
    http.end();
    systemMonitor.forceResetWatchdog();
    
    debugPrintf(COLOR_CYAN, "DEBUG: About to validate download - size=%d, expected=%d", bytesRead, size);
    
    // Validate download completeness
    if (bytesRead < size) {
        debugPrintf(COLOR_RED, "Incomplete download: %d/%d bytes", bytesRead, size);
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    // Additional validation: ensure we have enough data for JPEG processing
    if (bytesRead < 1024) {  // Minimum reasonable JPEG size
        debugPrintf(COLOR_RED, "Downloaded data too small: %d bytes", bytesRead);
        systemMonitor.forceResetWatchdog();
        return;
    }
    
    debugPrint("DEBUG: Download validation passed", COLOR_GREEN);
    
    // Reset watchdog before JPEG processing
    systemMonitor.forceResetWatchdog();
    
    // Decode and display JPEG
    debugPrint("Decoding JPEG...", COLOR_YELLOW);
    
    // Check JPEG header first
    if (bytesRead < 10) {
        debugPrintf(COLOR_RED, "ERROR: Downloaded data too small: %d bytes", bytesRead);
        return;
    }
    
    debugPrintf(COLOR_CYAN, "DEBUG: First 4 bytes: 0x%02X%02X%02X%02X", 
               imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
    
    // Check for PNG magic numbers first
    if (imageBuffer[0] == 0x89 && imageBuffer[1] == 0x50 && imageBuffer[2] == 0x4E && imageBuffer[3] == 0x47) {
        debugPrint("ERROR: PNG format detected - not supported (JPEG only)", COLOR_RED);
        Serial.println("This device only supports JPEG images. Please use a JPEG format image URL.");
        return;
    }
    
    // Check for JPEG magic numbers
    if (imageBuffer[0] != 0xFF || imageBuffer[1] != 0xD8) {
        debugPrintf(COLOR_RED, "ERROR: Invalid JPEG header: 0x%02X%02X (expected 0xFFD8)", imageBuffer[0], imageBuffer[1]);
        // Print first few bytes for debugging
        Serial.print("First bytes: ");
        for (int i = 0; i < min(16, (int)bytesRead); i++) {
            Serial.printf("0x%02X ", imageBuffer[i]);
        }
        Serial.println();
        Serial.println("ERROR: File format not supported. Please use JPEG images only.");
        return;
    }
    
    debugPrint("DEBUG: JPEG header validation passed", COLOR_GREEN);
    
    debugPrint("DEBUG: Attempting to open JPEG with openRAM", COLOR_CYAN);
    
    if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
        fullImageWidth = jpeg.getWidth();
        fullImageHeight = jpeg.getHeight();
        
        debugPrintf(COLOR_WHITE, "JPEG: %dx%d pixels", fullImageWidth, fullImageHeight);
        
        // Check if image fits in our full image buffer
        size_t requiredSize = fullImageWidth * fullImageHeight * 2;
        debugPrintf(COLOR_CYAN, "DEBUG: Required buffer size: %d, available: %d", requiredSize, fullImageBufferSize);
        
        if (requiredSize <= fullImageBufferSize) {
            debugPrint("Decoding to full buffer...", COLOR_YELLOW);
            
            // Clear the full image buffer
            memset(fullImageBuffer, 0, fullImageWidth * fullImageHeight * sizeof(uint16_t));
            
            // Decode the full image into our buffer with watchdog protection
            debugPrint("Starting JPEG decode...", COLOR_YELLOW);
            systemMonitor.forceResetWatchdog();
            
            unsigned long decodeStart = millis();
            
            // JPEG decode with timeout monitoring
            const unsigned long DECODE_TIMEOUT = 5000;  // 5 second timeout for decode
            unsigned long decodeStartTime = millis();
            
            debugPrint("DEBUG: Calling jpeg.decode()", COLOR_CYAN);
            
            if (jpeg.decode(0, 0, 0)) {
                unsigned long decodeTime = millis() - decodeStart;
                debugPrintf(COLOR_WHITE, "JPEG decode completed in %lu ms", decodeTime);
                debugPrint("Rendering image...", COLOR_GREEN);
                
                // Reset watchdog before rendering
                systemMonitor.forceResetWatchdog();
                
                debugPrint("DEBUG: Calling renderFullImage()", COLOR_CYAN);
                
                // Now render the full image with transformations
                renderFullImage();
                
                // Reset watchdog after rendering
                systemMonitor.forceResetWatchdog();
                
                // Mark first image as loaded (only once)
                if (!firstImageLoaded) {
                    firstImageLoaded = true;
                    displayManager.setFirstImageLoaded(true);
                    Serial.println("First image loaded successfully - switching to image mode");
                    debugPrint("DEBUG: First image loaded flag set", COLOR_GREEN);
                } else {
                    Serial.println("Image updated successfully");
                    debugPrint("DEBUG: Image updated successfully", COLOR_GREEN);
                }
            } else {
                debugPrint("ERROR: JPEG decode() function failed!", COLOR_RED);
            }
            
            jpeg.close();
            systemMonitor.forceResetWatchdog();
        } else {
            debugPrintf(COLOR_RED, "ERROR: Image too large for buffer! Required: %d, Available: %d", 
                       requiredSize, fullImageBufferSize);
            jpeg.close();
        }
    } else {
        debugPrint("ERROR: JPEG openRAM() failed!", COLOR_RED);
        debugPrintf(COLOR_RED, "Downloaded %d bytes, buffer size: %d", bytesRead, imageBufferSize);
    }
    
    // Final watchdog reset and cleanup
    systemMonitor.forceResetWatchdog();
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    debugPrint("Download cycle completed", COLOR_GREEN);
}

// Load cycling configuration from storage
void loadCyclingConfiguration() {
    cyclingEnabled = configStorage.getCyclingEnabled();
    currentCycleInterval = configStorage.getCycleInterval();
    randomOrderEnabled = configStorage.getRandomOrder();
    currentImageIndex = configStorage.getCurrentImageIndex();
    imageSourceCount = configStorage.getImageSourceCount();
    
    Serial.printf("Cycling config loaded: enabled=%s, interval=%lu ms, random=%s, sources=%d\n",
                  cyclingEnabled ? "true" : "false", currentCycleInterval,
                  randomOrderEnabled ? "true" : "false", imageSourceCount);
}

// Update cycling variables from configuration storage
void updateCyclingVariables() {
    loadCyclingConfiguration();
}

// Advance to next image in cycling sequence
void advanceToNextImage() {
    if (!cyclingEnabled || imageSourceCount <= 1) {
        return;
    }
    
    if (randomOrderEnabled) {
        // Random order: pick a different random image
        int newIndex;
        do {
            newIndex = random(0, imageSourceCount);
        } while (newIndex == currentImageIndex && imageSourceCount > 1);
        currentImageIndex = newIndex;
    } else {
        // Sequential order: advance to next image
        currentImageIndex = (currentImageIndex + 1) % imageSourceCount;
    }
    
    // Save the new index to persistent storage
    configStorage.setCurrentImageIndex(currentImageIndex);
    configStorage.saveConfig();
    
    Serial.printf("Advanced to image %d/%d: %s\n", 
                  currentImageIndex + 1, imageSourceCount, getCurrentImageURL().c_str());
}

// Get current image URL based on cycling configuration
String getCurrentImageURL() {
    if (cyclingEnabled && imageSourceCount > 0) {
        String url = configStorage.getImageSource(currentImageIndex);
        if (url.length() > 0) {
            return url;
        }
    }
    
    // Fallback to legacy single image URL
    return configStorage.getImageURL();
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
                
            // Reboot command
            case 'B':
            case 'b':
                Serial.println("Rebooting device...");
                delay(1000); // Give time for message to be sent
                ESP.restart();
                break;
                
            // Help command
            case 'H':
            case 'h':
            case '?':
                Serial.println("\n=== Image Control Commands ===");
                Serial.println("Scaling:");
                Serial.println("  +/- : Scale both axes");
                Serial.println("Movement:");
                Serial.println("  W/S : Move up/down");
                Serial.println("  A/D : Move left/right");
                Serial.println("Rotation:");
                Serial.println("  Q/E : Rotate 90° CCW/CW");
                Serial.println("Reset:");
                Serial.println("  R   : Reset all transformations");
                Serial.println("Brightness:");
                Serial.println("  L/K : Brightness up/down");
                Serial.println("System:");
                Serial.println("  B   : Reboot device");
                Serial.println("  M   : Memory info");
                Serial.println("  I   : Network info");
                Serial.println("  P   : PPA info");
                Serial.println("  T   : MQTT info");
                Serial.println("  X   : Web server status/restart");
                Serial.println("Touch:");
                Serial.println("  Single tap : Next image");
                Serial.println("  Double tap : Toggle cycling/single refresh mode");
                Serial.println("Help:");
                Serial.println("  H/? : Show this help");
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
            case 'X':
            case 'x':
                // Web server status and restart
                Serial.println("\n=== Web Server Status ===");
                Serial.printf("WiFi connected: %s\n", wifiManager.isConnected() ? "YES" : "NO");
                if (wifiManager.isConnected()) {
                    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
                }
                Serial.printf("Web server running: %s\n", webConfig.isRunning() ? "YES" : "NO");
                
                // Try to restart web server
                if (wifiManager.isConnected()) {
                    Serial.println("Attempting to restart web server...");
                    webConfig.stop();
                    delay(500);
                    if (webConfig.begin(8080)) {
                        Serial.printf("Web server restarted successfully at: http://%s:8080\n", WiFi.localIP().toString().c_str());
                    } else {
                        Serial.println("ERROR: Failed to restart web server");
                    }
                } else {
                    Serial.println("Cannot start web server - WiFi not connected");
                }
                break;
        }
        
        // Clear any remaining characters
        while (Serial.available()) {
            Serial.read();
        }
    }
}

void loop() {
    // Force watchdog reset at start of each loop iteration
    systemMonitor.forceResetWatchdog();
    
    // Handle web server first for maximum responsiveness
    if (wifiManager.isConnected() && webConfig.isRunning()) {
        // Add debugging for web server activity
        unsigned long webHandleStart = millis();
        webConfig.handleClient();
        unsigned long webHandleTime = millis() - webHandleStart;
        
        // Log if web server handling takes significant time (may indicate activity)
        if (webHandleTime > 10) {
            Serial.printf("DEBUG: Web server handling took %lu ms (potential request processed)\n", webHandleTime);
        }
    }
    
    unsigned long loopStartTime = millis();
    
    // Update all system modules with watchdog protection
    systemMonitor.update();
    systemMonitor.forceResetWatchdog();
    
    wifiManager.update();
    systemMonitor.forceResetWatchdog();
    
    // Handle web configuration server with better error handling
    if (wifiManager.isConnected()) {
        if (webConfig.isRunning()) {
            unsigned long webStartTime = millis();
            
            // Handle web requests without timeout restrictions that might interrupt connections
            webConfig.handleClient();
            
            unsigned long webHandleTime = millis() - webStartTime;
            
            // Only warn if it takes excessively long (increased threshold)
            if (webHandleTime > 5000) {
                Serial.printf("WARNING: Web client handling took %lu ms\n", webHandleTime);
            }
            
            // Don't reset watchdog immediately after web handling to prevent connection interruption
            // systemMonitor.forceResetWatchdog();
        } else {
            // Try to start web server if not running
            Serial.println("DEBUG: Web server not running, attempting to restart...");
            if (webConfig.begin(8080)) {
                Serial.printf("Web configuration server restarted at: http://%s:8080\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("ERROR: Failed to restart web configuration server");
            }
        }
        systemMonitor.forceResetWatchdog();
    }
    
    // Update MQTT manager (connect to MQTT after WiFi is established) with protection
    if (wifiManager.isConnected()) {
        unsigned long mqttStartTime = millis();
        mqttManager.update();
        
        // Check if MQTT update took too long
        if (millis() - mqttStartTime > 2000) {
            Serial.printf("WARNING: MQTT update took %lu ms\n", millis() - mqttStartTime);
        }
        systemMonitor.forceResetWatchdog();
    }
    
    // Additional web server handling to prevent empty responses
    if (wifiManager.isConnected() && webConfig.isRunning()) {
        webConfig.handleClient();
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
    
    // Update touch state and process gestures
    if (touchEnabled) {
        updateTouchState();
        processTouchGestures();
        systemMonitor.forceResetWatchdog();
    }
    
    // Handle web server again after serial processing
    if (wifiManager.isConnected() && webConfig.isRunning()) {
        webConfig.handleClient();
    }
    
    // Enhanced stuck image processing detection
    if (imageProcessing && (millis() - lastImageProcessTime > IMAGE_PROCESS_TIMEOUT)) {
        Serial.printf("WARNING: Image processing timeout detected after %lu ms, resetting...\n", 
                     millis() - lastImageProcessTime);
        imageProcessing = false;
        systemMonitor.forceResetWatchdog();
        
        // Log system state for debugging
        Serial.printf("DEBUG: Loop has been running for %lu ms\n", millis() - loopStartTime);
        Serial.printf("DEBUG: Free heap: %d, Free PSRAM: %d\n", 
                     systemMonitor.getCurrentFreeHeap(), systemMonitor.getCurrentFreePsram());
    }
    
    // Update dynamic configuration
    currentUpdateInterval = configStorage.getUpdateInterval();
    systemMonitor.forceResetWatchdog();
    
    // Check for touch-triggered actions
    if (touchTriggeredNextImage) {
        touchTriggeredNextImage = false;
        Serial.println("Touch: Advancing to next image");
        debugPrint("Touch: Next image requested", COLOR_CYAN);
        
        // If cycling is enabled, advance to next image
        if (cyclingEnabled && imageSourceCount > 1) {
            advanceToNextImage();
            // Force immediate image download
            lastUpdate = 0;
        } else {
            Serial.println("Touch: Cycling not enabled or only one source configured");
            debugPrint("Touch: Single image mode - cannot advance", COLOR_YELLOW);
        }
        systemMonitor.forceResetWatchdog();
    }
    
    if (touchTriggeredModeToggle) {
        touchTriggeredModeToggle = false;
        
        // Toggle between cycling mode and single image refresh mode
        singleImageRefreshMode = !singleImageRefreshMode;
        
        if (singleImageRefreshMode) {
            Serial.println("Touch: Switched to SINGLE IMAGE REFRESH mode");
            debugPrint("Mode: Single Image Refresh (no auto-cycling)", COLOR_MAGENTA);
            Serial.printf("Current image will refresh every %lu minutes\n", currentUpdateInterval / 60000);
        } else {
            Serial.println("Touch: Switched to CYCLING mode");
            debugPrint("Mode: Auto-Cycling Enabled", COLOR_MAGENTA);
            Serial.printf("Will cycle every %lu minutes, update every %lu minutes\n", 
                         currentCycleInterval / 60000, currentUpdateInterval / 60000);
        }
        systemMonitor.forceResetWatchdog();
    }
    
    // Check for image cycling (independent of update interval)
    unsigned long currentTime = millis();
    bool shouldCycle = false;
    
    // Only auto-cycle if not in single image refresh mode
    if (cyclingEnabled && imageSourceCount > 1 && !imageProcessing && !singleImageRefreshMode) {
        if (currentTime - lastCycleTime >= currentCycleInterval || lastCycleTime == 0) {
            shouldCycle = true;
            lastCycleTime = currentTime;
            Serial.println("DEBUG: Time to cycle to next image source");
            advanceToNextImage();
        }
    }
    
    // Check if it's time to update the image (either scheduled update or cycling)
    if (!imageProcessing && (shouldCycle || currentTime - lastUpdate >= currentUpdateInterval || lastUpdate == 0)) {
        // Pre-download system health check
        if (!wifiManager.isConnected()) {
            Serial.println("WARNING: WiFi disconnected, skipping image download");
            lastUpdate = currentTime; // Update timestamp to prevent immediate retry
            systemMonitor.forceResetWatchdog();
        } else {
            Serial.printf("DEBUG: Starting image download cycle (last update: %lu ms ago)\n", 
                         currentTime - lastUpdate);
            
            imageProcessing = true;
            lastImageProcessTime = currentTime;
            systemMonitor.forceResetWatchdog();
            
            // Wrap download in timeout protection with absolute timeout
            unsigned long downloadStartTime = millis();
            const unsigned long ABSOLUTE_DOWNLOAD_TIMEOUT = 20000; // 20 second absolute timeout
            
            // Create a flag to track download completion
            bool downloadCompleted = false;
            
            // Try the download with timeout monitoring
            unsigned long downloadCheckTime = millis();
            while (!downloadCompleted && (millis() - downloadStartTime) < ABSOLUTE_DOWNLOAD_TIMEOUT) {
                // Reset watchdog frequently during download attempt
                systemMonitor.forceResetWatchdog();
                
                // Try download
                downloadAndDisplayImage();
                downloadCompleted = true;
                
                // Check if we're taking too long
                if (millis() - downloadCheckTime > 1000) {
                    Serial.printf("DEBUG: Download attempt running for %lu ms\n", millis() - downloadStartTime);
                    downloadCheckTime = millis();
                }
            }
            
            unsigned long downloadDuration = millis() - downloadStartTime;
            
            if (downloadCompleted) {
                Serial.printf("DEBUG: Download cycle completed in %lu ms\n", downloadDuration);
                
                // Log warning if download took unusually long
                if (downloadDuration > 15000) {
                    Serial.printf("WARNING: Download cycle took %lu ms (unusually long)\n", downloadDuration);
                }
            } else {
                Serial.printf("ERROR: Download cycle timed out after %lu ms\n", downloadDuration);
                debugPrint("ERROR: Download timed out completely", COLOR_RED);
            }
            
            lastUpdate = currentTime;
            imageProcessing = false;
            systemMonitor.forceResetWatchdog();
        }
    }
    
    // Check total loop time for performance monitoring
    unsigned long loopDuration = millis() - loopStartTime;
    if (loopDuration > 1000) {
        Serial.printf("WARNING: Loop iteration took %lu ms\n", loopDuration);
    }
    
    // Additional watchdog reset before delay
    systemMonitor.forceResetWatchdog();
    
    // Handle web server once more before delay to ensure responsiveness
    if (wifiManager.isConnected() && webConfig.isRunning()) {
        webConfig.handleClient();
    }
    
    // Use shorter delay to keep web server responsive
    systemMonitor.safeDelay(50);
    systemMonitor.forceResetWatchdog();
}

// Touch controller functions implementation

void initializeTouchController() {
    debugPrint("Initializing touch controller...", COLOR_YELLOW);
    
    try {
        // Initialize I2C interface first
        DEV_I2C_Port i2cPort = DEV_I2C_Init();
        
        // Initialize the GT911 touch controller
        touchHandle = touch_gt911_init(i2cPort);
        
        if (touchHandle != nullptr) {
            touchEnabled = true;
            debugPrint("Touch controller initialized successfully!", COLOR_GREEN);
            Serial.println("GT911 touch controller ready");
        } else {
            debugPrint("Touch controller initialization failed", COLOR_RED);
            Serial.println("Warning: Touch functionality disabled - GT911 init failed");
            touchEnabled = false;
        }
    } catch (...) {
        debugPrint("Touch controller initialization exception", COLOR_RED);
        Serial.println("Warning: Touch functionality disabled - exception during init");
        touchEnabled = false;
    }
    
    // Initialize touch state variables
    touchState = TOUCH_IDLE;
    touchPressed = false;
    touchWasPressed = false;
    touchPressTime = 0;
    touchReleaseTime = 0;
    firstTapTime = 0;
    touchTriggeredNextImage = false;
    touchTriggeredModeToggle = false;
}

void updateTouchState() {
    if (!touchEnabled || touchHandle == nullptr) {
        return;
    }
    
    // Read touch data from GT911
    touch_gt911_point_t touchData = touch_gt911_read_point(1);  // Read max 1 touch point
    
    unsigned long currentTime = millis();
    bool currentlyPressed = (touchData.cnt > 0);
    
    // Debouncing: ignore rapid state changes
    if (currentlyPressed != touchPressed) {
        if (currentTime - touchReleaseTime < TOUCH_DEBOUNCE_MS && 
            currentTime - touchPressTime < TOUCH_DEBOUNCE_MS) {
            return; // Too soon since last state change
        }
    }
    
    // Update touch state
    touchWasPressed = touchPressed;
    touchPressed = currentlyPressed;
    
    // Handle touch press
    if (touchPressed && !touchWasPressed) {
        touchPressTime = currentTime;
        
        if (touchState == TOUCH_IDLE) {
            touchState = TOUCH_PRESSED;
            Serial.printf("Touch: Press detected at %lu ms\n", currentTime);
        } else if (touchState == TOUCH_WAITING_FOR_SECOND_TAP) {
            // Second tap detected within timeout
            if (currentTime - firstTapTime <= DOUBLE_TAP_TIMEOUT_MS) {
                touchState = TOUCH_PRESSED;
                Serial.printf("Touch: Second tap detected at %lu ms (delta: %lu ms)\n", 
                             currentTime, currentTime - firstTapTime);
            } else {
                // Timeout exceeded, treat as new first tap
                touchState = TOUCH_PRESSED;
                Serial.printf("Touch: Double-tap timeout, treating as new press at %lu ms\n", currentTime);
            }
        }
    }
    
    // Handle touch release
    if (!touchPressed && touchWasPressed) {
        touchReleaseTime = currentTime;
        unsigned long pressDuration = currentTime - touchPressTime;
        
        Serial.printf("Touch: Release detected at %lu ms (duration: %lu ms)\n", 
                     currentTime, pressDuration);
        
        if (touchState == TOUCH_PRESSED) {
            // Valid tap duration check
            if (pressDuration >= MIN_TAP_DURATION_MS && pressDuration <= MAX_TAP_DURATION_MS) {
                if (firstTapTime == 0) {
                    // First tap
                    firstTapTime = touchReleaseTime;
                    touchState = TOUCH_WAITING_FOR_SECOND_TAP;
                    Serial.printf("Touch: First tap completed, waiting for second tap\n");
                } else {
                    // Second tap
                    if (currentTime - firstTapTime <= DOUBLE_TAP_TIMEOUT_MS) {
                        // Valid double tap - toggle mode
                        touchTriggeredModeToggle = true;
                        touchState = TOUCH_IDLE;
                        firstTapTime = 0;
                        Serial.printf("Touch: Double-tap detected! (total time: %lu ms)\n", 
                                     currentTime - firstTapTime);
                    } else {
                        // Timeout exceeded, treat as single tap
                        touchTriggeredNextImage = true;
                        touchState = TOUCH_IDLE;
                        firstTapTime = 0;
                        Serial.printf("Touch: Double-tap timeout, treating as single tap\n");
                    }
                }
            } else {
                // Invalid tap duration (too short or too long)
                touchState = TOUCH_IDLE;
                firstTapTime = 0;
                Serial.printf("Touch: Invalid tap duration: %lu ms (min: %lu, max: %lu)\n", 
                             pressDuration, MIN_TAP_DURATION_MS, MAX_TAP_DURATION_MS);
            }
        }
    }
    
    // Check for double-tap timeout in waiting state
    if (touchState == TOUCH_WAITING_FOR_SECOND_TAP && firstTapTime > 0) {
        if (currentTime - firstTapTime > DOUBLE_TAP_TIMEOUT_MS) {
            // Timeout exceeded - trigger single tap action
            touchTriggeredNextImage = true;
            touchState = TOUCH_IDLE;
            firstTapTime = 0;
            Serial.printf("Touch: Double-tap timeout - triggering single tap action\n");
        }
    }
}

void processTouchGestures() {
    // This function is called from the main loop
    // The actual gesture processing is handled in updateTouchState()
    // Touch-triggered actions are processed in the main loop via flags
}

void handleSingleTap() {
    Serial.println("Touch: Single tap - advancing to next image");
    debugPrint("Touch: Single tap detected", COLOR_GREEN);
    
    // Set flag to trigger next image in main loop
    touchTriggeredNextImage = true;
}

void handleDoubleTap() {
    Serial.println("Touch: Double tap - toggling mode");
    debugPrint("Touch: Double tap detected", COLOR_GREEN);
    
    // Set flag to trigger mode toggle in main loop
    touchTriggeredModeToggle = true;
}
