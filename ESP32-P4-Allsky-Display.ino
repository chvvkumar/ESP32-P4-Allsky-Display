// Include all our modular components
#include "config.h"
#include "config_storage.h"
#include "logging.h"  // Logging macros and functions
#include "web_config.h"
#include "system_monitor.h"
#include "network_manager.h"
#include "captive_portal.h"
#include "display_manager.h"
#include "ppa_accelerator.h"
#include "mqtt_manager.h"
#include "ota_manager.h"
#include "gt911.h"
#include "i2c.h"
#include "task_retry_handler.h"
#include "wifi_qr_code.h"
#include "crash_logger.h"
#include "command_interpreter.h"
#include "image_utils.h"  // Software image scaling fallback

// Additional required libraries
#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>

// Global variables for image processing
bool firstImageLoaded = false;
unsigned long lastUpdate = 0;
uint8_t* imageBuffer = nullptr;
size_t imageBufferSize = 0;

// Image transformation variables
float scaleX = DEFAULT_SCALE_X;
float scaleY = DEFAULT_SCALE_Y;
int offsetX = DEFAULT_OFFSET_X;
int offsetY = DEFAULT_OFFSET_Y;
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

// Pending image buffer (downloaded/decoded but not yet displayed)
uint16_t* pendingFullImageBuffer = nullptr;
int16_t pendingImageWidth = 0;
int16_t pendingImageHeight = 0;
bool imageReadyToDisplay = false;  // Flag: new image fully prepared and ready to show

// Scaling buffer for transformed images
uint16_t* scaledBuffer = nullptr;
size_t scaledBufferSize = 0;

// Previous image tracking for seamless transitions
static int16_t prevImageX = -1, prevImageY = -1;
static int16_t prevImageWidth = 0, prevImageHeight = 0;
static bool hasSeenFirstImage = false;

// JPEG decoder
JPEGDEC jpeg;

// WiFi QR Code display
uint16_t* qrCodeBuffer = nullptr;
int16_t qrCodeWidth = 0;
int16_t qrCodeHeight = 0;

// Reusable scratch buffer for temporary operations (QR codes, etc.)
// 512KB buffer to prevent fragmentation
uint16_t* scratchBuffer = nullptr;

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

// Touch actions triggered flags
bool touchTriggeredNextImage = false;
bool touchTriggeredModeToggle = false;

// Touch mode control
bool singleImageRefreshMode = false;  // false = cycling mode, true = single image refresh mode

// =============================================================================
// WIFI SETUP MODE GLOBALS
// =============================================================================
// Non-blocking WiFi setup state management
bool wifiSetupMode = false;
unsigned long wifiSetupStartTime = 0;

// =============================================================================
// ASYNC DOWNLOAD GLOBALS
// =============================================================================
// FreeRTOS task handles and synchronization primitives for non-blocking downloads
TaskHandle_t downloadTaskHandle = nullptr;
QueueHandle_t imageReadyQueue = nullptr;
volatile bool imageDownloadPending = false;  // Flag to trigger download
SemaphoreHandle_t imageBufferMutex = nullptr;  // Protect buffer access

// Forward declarations
void debugPrint(const char* message, uint16_t color);
void debugPrintf(uint16_t color, const char* format, ...);
int JPEGDraw(JPEGDRAW *pDraw);
int JPEGDrawQR(JPEGDRAW *pDraw);
int16_t displayWiFiQRCode();
void downloadAndDisplayImage();
void renderFullImage();
void loadCyclingConfiguration();
void advanceToNextImage();
String getCurrentImageURL();
void updateCyclingVariables();
void updateCurrentImageTransformSettings();
void downloadTask(void* params);

// Touch function declarations
void initializeTouchController();
void updateTouchState();
void processTouchGestures();
void handleSingleTap();
void handleDoubleTap();

// Universal logging function - sends to Serial AND WebSocket console
// Default parameter defined in logging.h
// Note: broadcastLog() adds timestamps and severity prefixes automatically
void logPrint(const char* message, LogSeverity severity) {
    Serial.print(message);
    // Log to crash logger (survives reboot)
    crashLogger.log(message);
    // Send to WebSocket console clients with severity filtering (adds timestamp + severity prefix)
    webConfig.broadcastLog(message, COLOR_WHITE, severity);
}

void logPrintf(LogSeverity severity, const char* format, ...) {
    char buffer[384];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial.print(buffer);
    // Log to crash logger (survives reboot)
    crashLogger.log(buffer);
    // Send to WebSocket console clients with severity filtering (adds timestamp + severity prefix)
    webConfig.broadcastLog(buffer, COLOR_WHITE, severity);
}

// Debug output wrapper functions (also show on display)
// Note: broadcastLog() adds timestamps and severity prefixes automatically
void debugPrint(const char* message, uint16_t color) {
    displayManager.debugPrint(message, color);
    
    // Log to crash logger (survives reboot)
    crashLogger.log(message);
    crashLogger.log("\n");
    
    // Intelligently detect severity from message content
    LogSeverity severity = LOG_DEBUG;  // Default to DEBUG for debugPrint calls
    String msg = String(message);
    msg.toLowerCase();
    
    if (msg.indexOf("info") >= 0 || msg.indexOf("✓") >= 0) {
        severity = LOG_INFO;
    } else if (msg.indexOf("error") >= 0 || msg.indexOf("fail") >= 0 || msg.indexOf("✗") >= 0) {
        severity = LOG_ERROR;
    } else if (msg.indexOf("warning") >= 0 || msg.indexOf("warn") >= 0) {
        severity = LOG_WARNING;
    } else if (msg.indexOf("critical") >= 0 || msg.indexOf("fatal") >= 0 || msg.indexOf("panic") >= 0) {
        severity = LOG_CRITICAL;
    }
    
    // Send to WebSocket console clients with detected severity (broadcastLog adds timestamp)
    webConfig.broadcastLog(message, color, severity);
}

void debugPrintf(uint16_t color, const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    displayManager.debugPrint(buffer, color);
    
    // Log to crash logger (survives reboot)
    crashLogger.log(buffer);
    crashLogger.log("\n");
    
    // Intelligently detect severity from message content
    LogSeverity severity = LOG_DEBUG;  // Default to DEBUG for debugPrintf calls
    String msg = String(buffer);
    msg.toLowerCase();
    
    if (msg.indexOf("info") >= 0 || msg.indexOf("✓") >= 0) {
        severity = LOG_INFO;
    } else if (msg.indexOf("error") >= 0 || msg.indexOf("fail") >= 0 || msg.indexOf("✗") >= 0) {
        severity = LOG_ERROR;
    } else if (msg.indexOf("warning") >= 0 || msg.indexOf("warn") >= 0) {
        severity = LOG_WARNING;
    } else if (msg.indexOf("critical") >= 0 || msg.indexOf("fatal") >= 0 || msg.indexOf("panic") >= 0) {
        severity = LOG_CRITICAL;
    }
    
    // Send to WebSocket console clients with detected severity (broadcastLog adds timestamp)
    webConfig.broadcastLog(buffer, color, severity);
}

// JPEG callback function to collect pixels into PENDING image buffer (not displayed yet)
int JPEGDraw(JPEGDRAW *pDraw) {
    // Store pixels in the PENDING image buffer (will be swapped to active buffer when complete)
    if (pendingFullImageBuffer && pDraw->y + pDraw->iHeight <= pendingImageHeight) {
        for (int16_t y = 0; y < pDraw->iHeight; y++) {
            uint16_t* destRow = pendingFullImageBuffer + ((pDraw->y + y) * pendingImageWidth + pDraw->x);
            uint16_t* srcRow = pDraw->pPixels + (y * pDraw->iWidth);
            memcpy(destRow, srcRow, pDraw->iWidth * sizeof(uint16_t));
        }
    }
    return 1;
}

// JPEG callback function for QR code display
int JPEGDrawQR(JPEGDRAW *pDraw) {
    // Store pixels in the QR code buffer
    if (qrCodeBuffer && pDraw->y + pDraw->iHeight <= qrCodeHeight) {
        for (int16_t y = 0; y < pDraw->iHeight; y++) {
            uint16_t* destRow = qrCodeBuffer + ((pDraw->y + y) * qrCodeWidth + pDraw->x);
            uint16_t* srcRow = pDraw->pPixels + (y * pDraw->iWidth);
            memcpy(destRow, srcRow, pDraw->iWidth * sizeof(uint16_t));
        }
    }
    return 1;
}

// Display WiFi QR Code on screen (called during WiFi setup)
// Returns the Y position where text should start
int16_t displayWiFiQRCode() {
    // CRITICAL: Reset watchdog immediately - this function takes a long time
    systemMonitor.forceResetWatchdog();
    
    int16_t displayWidth = displayManager.getWidth();
    int16_t displayHeight = displayManager.getHeight();
    int16_t textStartY = 200; // Default fallback
    
    LOG_DEBUG("DEBUG: Loading WiFi QR code...");
    
    // Reset watchdog before system calls
    systemMonitor.forceResetWatchdog();
    
    LOG_DEBUG_F("DEBUG: Free heap before QR: %d bytes\n", systemMonitor.getCurrentFreeHeap());
    LOG_DEBUG_F("DEBUG: Free PSRAM before QR: %d bytes\n", systemMonitor.getCurrentFreePsram());
    
    // Reset watchdog before heavy operation
    systemMonitor.forceResetWatchdog();
    
    // First, open JPEG to get actual dimensions
    if (!jpeg.openRAM((uint8_t*)wifi_qr_code_jpg, wifi_qr_code_jpg_len, JPEGDrawQR)) {
        LOG_ERROR("ERROR: Failed to open QR code JPEG");
        return textStartY; // Return default position
    }
    
    // Reset watchdog after opening JPEG
    systemMonitor.forceResetWatchdog();
    
    qrCodeWidth = jpeg.getWidth();
    qrCodeHeight = jpeg.getHeight();
    LOG_DEBUG_F("DEBUG: QR code dimensions - %dx%d\n", qrCodeWidth, qrCodeHeight);
    
    // Check if QR code fits on display
    if (qrCodeWidth > displayWidth || qrCodeHeight > displayHeight) {
        LOG_ERROR("ERROR: QR code too large for display");
        jpeg.close();
        return textStartY; // Return default position
    }
    
    // Use scratch buffer instead of allocating new memory
    const size_t qrBufferSize = qrCodeWidth * qrCodeHeight * sizeof(uint16_t);
    const size_t scratchBufferSize = 512 * 512 * 2;  // 512KB
    
    if (!scratchBuffer) {
        LOG_ERROR("ERROR: Scratch buffer not available for QR code");
        jpeg.close();
        return textStartY; // Return default position
    }
    
    if (qrBufferSize > scratchBufferSize) {
        LOG_ERROR_F("ERROR: QR code too large for scratch buffer: %d bytes > %d bytes\n", 
                   qrBufferSize, scratchBufferSize);
        jpeg.close();
        return textStartY;
    }
    
    // Reuse scratch buffer for QR code (no allocation needed!)
    qrCodeBuffer = scratchBuffer;
    
    LOG_DEBUG_F("DEBUG: Using scratch buffer for QR code: %d bytes\n", qrBufferSize);
    
    // Reset watchdog after allocation
    systemMonitor.forceResetWatchdog();
    
    // Clear buffer
    memset(qrCodeBuffer, 0, qrBufferSize);
    
    // Reset watchdog before decode
    systemMonitor.forceResetWatchdog();
    
    // Pause display during decode to prevent memory contention
    displayManager.pauseDisplay();
    
    // Decode QR code JPEG
    if (jpeg.decode(0, 0, 0)) {
        LOG_DEBUG("DEBUG: QR code decoded successfully");
        
        // Reset watchdog after decode
        systemMonitor.forceResetWatchdog();
        
        // Resume display before drawing
        displayManager.resumeDisplay();
        
        // Scale down QR code to 65% for better fit on round display
        // Use PPA for hardware-accelerated scaling
        const float scale = 0.65f;
        int16_t scaledWidth = qrCodeWidth * scale;
        int16_t scaledHeight = qrCodeHeight * scale;
        
        // Allocate scaled buffer
        size_t scaledSize = scaledWidth * scaledHeight * sizeof(uint16_t);
        uint16_t* scaledQR = (uint16_t*)ps_malloc(scaledSize);
        
        // Reset watchdog after allocation
        systemMonitor.forceResetWatchdog();
        
        if (scaledQR) {
            LOG_DEBUG_F("DEBUG: Scaling QR from %dx%d to %dx%d\n", qrCodeWidth, qrCodeHeight, scaledWidth, scaledHeight);
            
            // Clear buffer before scaling
            memset(scaledQR, 0, scaledSize);
            
            // Reset watchdog before PPA scaling
            systemMonitor.forceResetWatchdog();
            
            // Use PPA to scale down
            if (ppaAccelerator.scaleImage(qrCodeBuffer, qrCodeWidth, qrCodeHeight,
                                         scaledQR, scaledWidth, scaledHeight)) {
                LOG_DEBUG("DEBUG: QR code scaled successfully");
                
                // Reset watchdog after scaling
                systemMonitor.forceResetWatchdog();
                
                // Position at top-center of screen with margin
                int16_t qrX = (displayWidth - scaledWidth) / 2;
                int16_t qrY = 50;  // Smaller margin from top
                
                // Clear area around QR code with margin to prevent corruption
                int16_t clearMargin = 5;
                Arduino_DSI_Display* gfx = displayManager.getGFX();
                if (gfx) {
                    gfx->fillRect(qrX - clearMargin, qrY - clearMargin, 
                                 scaledWidth + (clearMargin * 2), 
                                 scaledHeight + (clearMargin * 2), 
                                 COLOR_BLACK);
                }
                
                // Draw scaled QR code
                displayManager.drawBitmap(qrX, qrY, scaledQR, scaledWidth, scaledHeight);
                
                // Flush to ensure QR code is visible immediately
                if (gfx) {
                    gfx->flush();
                }
                
                LOG_DEBUG_F("DEBUG: Scaled QR code drawn and flushed at (%d, %d)\n", qrX, qrY);
                
                // Calculate where text should start (below QR code + small margin)
                textStartY = qrY + scaledHeight + 15;
            } else {
                LOG_WARNING("WARNING: PPA scaling failed, drawing original size");
                int16_t qrX = (displayWidth - qrCodeWidth) / 2;
                int16_t qrY = 50;
                
                // Clear area before drawing
                Arduino_DSI_Display* gfx = displayManager.getGFX();
                if (gfx) {
                    gfx->fillRect(qrX - 5, qrY - 5, qrCodeWidth + 10, qrCodeHeight + 10, COLOR_BLACK);
                }
                
                displayManager.drawBitmap(qrX, qrY, qrCodeBuffer, qrCodeWidth, qrCodeHeight);
                
                // Flush to ensure QR code is visible
                if (gfx) {
                    gfx->flush();
                }
                
                textStartY = qrY + qrCodeHeight + 15;
            }
            
            free(scaledQR);
        } else {
            LOG_WARNING("WARNING: Could not allocate scaled buffer, drawing original");
            int16_t qrX = (displayWidth - qrCodeWidth) / 2;
            int16_t qrY = 50;
            
            // Clear area before drawing
            Arduino_DSI_Display* gfx = displayManager.getGFX();
            if (gfx) {
                gfx->fillRect(qrX - 5, qrY - 5, qrCodeWidth + 10, qrCodeHeight + 10, COLOR_BLACK);
            }
            
            displayManager.drawBitmap(qrX, qrY, qrCodeBuffer, qrCodeWidth, qrCodeHeight);
            
            // Flush to ensure QR code is visible
            if (gfx) {
                gfx->flush();
            }
            
            textStartY = qrY + qrCodeHeight + 15;
        }
        
        // Reset watchdog after draw
        systemMonitor.forceResetWatchdog();
    } else {
        Serial.println("ERROR: QR code decode failed");
        displayManager.resumeDisplay();
    }
    
    jpeg.close();
    
    // No need to free - we're using the reusable scratch buffer
    qrCodeBuffer = nullptr;  // Just clear the pointer
    Serial.println("DEBUG: QR buffer released (scratch buffer reusable)");
    
    Serial.printf("DEBUG: Free heap after QR: %d bytes\n", systemMonitor.getCurrentFreeHeap());
    Serial.printf("DEBUG: Free PSRAM after QR: %d bytes\n", systemMonitor.getCurrentFreePsram());
    
    // Final watchdog reset
    systemMonitor.forceResetWatchdog();
    
    return textStartY;
}

void setup() {
    // CRITICAL: Disable bootloader watchdog immediately if it exists
    // ESP32 bootloader may start a watchdog timer before setup() runs
    // We need to disable it so we can reconfigure with proper timeout for heavy initialization
    esp_err_t wdt_status = esp_task_wdt_deinit();
    
    Serial.begin(9600);
    
    // Small delay for serial stability (reduced from 1000ms to minimize boot time)
    delay(500);
    
    // Initialize crash logger FIRST to capture ALL boot messages
    crashLogger.begin();
    
    // Check if last boot was a crash
    if (crashLogger.wasLastBootCrash()) {
        Serial.println("\n**************************************************");
        Serial.println("***  WARNING: PREVIOUS BOOT ENDED IN CRASH!   ***");
        Serial.println("***  Crash logs preserved in RTC memory/NVS   ***");
        Serial.println("**************************************************\n");
    }
    
    // Initialize configuration system first
    initializeConfiguration();
    
    // Initialize system monitor with configured watchdog timeout
    // This will properly configure the watchdog with our desired timeout
    unsigned long watchdogTimeout = configStorage.getWatchdogTimeout();
    LOG_DEBUG_F("Initializing system monitor with watchdog timeout: %lu ms\n", watchdogTimeout);
    if (!systemMonitor.begin(watchdogTimeout)) {
        LOG_CRITICAL("CRITICAL: System monitor initialization failed!");
        while(1) delay(1000);
    }
    
    // Now watchdog is properly configured - reset it after initialization
    systemMonitor.forceResetWatchdog();
    
    // Pre-allocate all PSRAM buffers BEFORE display init to ensure enough contiguous memory
    // Display needs ~1.28MB contiguous for frame buffer (800x800x2), so we allocate our buffers first
    LOG_DEBUG("Pre-allocating PSRAM buffers before display initialization...");
    LOG_DEBUG_F("Free PSRAM before allocation: %d bytes\n", ESP.getFreePsram());
    
    // Calculate buffer sizes based on display configuration
    const int16_t w = display_cfg.width;
    const int16_t h = display_cfg.height;
    LOG_DEBUG_F("Display dimensions from config: %dx%d\n", w, h);
    
    LOG_DEBUG("[Memory] Allocating image buffers...");
    LOG_DEBUG_F("[Memory] Pre-allocation state: Heap=%d bytes, PSRAM=%d bytes\n", 
                 ESP.getFreeHeap(), ESP.getFreePsram());
    
    // Clean up any existing allocations (safety check for repeated setup calls)
    if (imageBuffer) { free(imageBuffer); imageBuffer = nullptr; }
    if (fullImageBuffer) { free(fullImageBuffer); fullImageBuffer = nullptr; }
    if (pendingFullImageBuffer) { free(pendingFullImageBuffer); pendingFullImageBuffer = nullptr; }
    if (scaledBuffer) { free(scaledBuffer); scaledBuffer = nullptr; }
    
    imageBufferSize = w * h * IMAGE_BUFFER_MULTIPLIER * 2;
    LOG_DEBUG_F("[Memory] Allocating image buffer: %d bytes (%.1f KB)\n", 
                 imageBufferSize, imageBufferSize / 1024.0);
    imageBuffer = (uint8_t*)ps_malloc(imageBufferSize);
    if (!imageBuffer) {
        LOG_CRITICAL("[Memory] ✗ CRITICAL: Image buffer allocation failed!\n");
        LOG_CRITICAL_F("CRITICAL: Image buffer pre-allocation failed! Size: %d bytes\n", imageBufferSize);
        LOG_CRITICAL_F("[Memory] Free PSRAM: %d bytes (need %d)\n", ESP.getFreePsram(), imageBufferSize);
        LOG_CRITICAL_F("[Memory] Free Heap: %d bytes\n", ESP.getFreeHeap());
        LOG_CRITICAL_F("[Memory] Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        crashLogger.saveBeforeReboot();
        delay(100);
        ESP.restart();
    }
    LOG_DEBUG_F("[Memory] ✓ Image buffer: %d bytes (%.1f KB)\n", imageBufferSize, imageBufferSize / 1024.0);
    LOG_DEBUG_F("✓ Image buffer allocated: %d bytes\n", imageBufferSize);
    
    fullImageBufferSize = FULL_IMAGE_BUFFER_SIZE;
    LOG_DEBUG_F("[Memory] Allocating full image buffer: %d bytes (%.1f KB, max 512x512)\n", 
                 fullImageBufferSize, fullImageBufferSize / 1024.0);
    fullImageBuffer = (uint16_t*)ps_malloc(fullImageBufferSize);
    if (!fullImageBuffer) {
        LOG_CRITICAL("[Memory] ✗ CRITICAL: Full image buffer allocation failed!\n");
        LOG_CRITICAL_F("CRITICAL: Full image buffer pre-allocation failed! Size: %d bytes\n", fullImageBufferSize);
        LOG_CRITICAL_F("[Memory] Free PSRAM: %d bytes (need %d)\n", ESP.getFreePsram(), fullImageBufferSize);
        LOG_CRITICAL_F("[Memory] Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        crashLogger.saveBeforeReboot();
        delay(100);
        ESP.restart();
    }
    LOG_DEBUG_F("[Memory] ✓ Full image buffer: %d bytes (%.1f KB)\n", fullImageBufferSize, fullImageBufferSize / 1024.0);
    LOG_DEBUG_F("✓ Full image buffer allocated: %d bytes\n", fullImageBufferSize);
    
    LOG_DEBUG_F("[Memory] Allocating pending buffer: %d bytes (double-buffer for flicker-free)\n", 
                 fullImageBufferSize);
    pendingFullImageBuffer = (uint16_t*)ps_malloc(fullImageBufferSize);
    if (!pendingFullImageBuffer) {
        LOG_CRITICAL("[Memory] ✗ CRITICAL: Pending buffer allocation failed!\n");
        LOG_CRITICAL_F("CRITICAL: Pending image buffer pre-allocation failed! Size: %d bytes\n", fullImageBufferSize);
        LOG_CRITICAL_F("[Memory] Free PSRAM: %d bytes (need %d)\n", ESP.getFreePsram(), fullImageBufferSize);
        LOG_CRITICAL_F("[Memory] Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        crashLogger.saveBeforeReboot();
        delay(100);
        ESP.restart();
    }
    LOG_DEBUG_F("[Memory] ✓ Pending buffer: %d bytes (%.1f KB)\n", fullImageBufferSize, fullImageBufferSize / 1024.0);
    LOG_DEBUG_F("✓ Pending image buffer allocated: %d bytes\n", fullImageBufferSize);
    
    scaledBufferSize = w * h * SCALED_BUFFER_MULTIPLIER * 2;
    LOG_DEBUG_F("[Memory] Allocating scaled buffer: %d bytes (%.1f KB, 4x display for PPA)\n", 
                 scaledBufferSize, scaledBufferSize / 1024.0);
    scaledBuffer = (uint16_t*)ps_malloc(scaledBufferSize);
    if (!scaledBuffer) {
        LOG_CRITICAL("[Memory] ✗ CRITICAL: Scaled buffer allocation failed!\n");
        LOG_CRITICAL_F("CRITICAL: Scaled buffer pre-allocation failed! Size: %d bytes\n", scaledBufferSize);
        LOG_CRITICAL_F("[Memory] Free PSRAM: %d bytes (need %d)\n", ESP.getFreePsram(), scaledBufferSize);
        LOG_CRITICAL_F("[Memory] Largest free block: %d bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        crashLogger.saveBeforeReboot();
        delay(100);
        ESP.restart();
    }
    LOG_DEBUG_F("[Memory] ✓ Scaled buffer: %d bytes (%.1f KB)\n", scaledBufferSize, scaledBufferSize / 1024.0);
    LOG_DEBUG_F("✓ Scaled buffer allocated: %d bytes\n", scaledBufferSize);
    
    // Allocate reusable scratch buffer for QR codes and other temporary operations
    size_t scratchBufferSize = 512 * 512 * 2;  // 512KB buffer
    LOG_DEBUG_F("[Memory] Allocating scratch buffer: %d bytes (%.1f KB, reusable)\n", 
                 scratchBufferSize, scratchBufferSize / 1024.0);
    scratchBuffer = (uint16_t*)ps_malloc(scratchBufferSize);
    if (!scratchBuffer) {
        LOG_WARNING("[Memory] ✗ WARNING: Scratch buffer allocation failed - QR codes may not work\n");
        LOG_WARNING_F("[Memory] Free PSRAM: %d bytes\n", ESP.getFreePsram());
    } else {
        LOG_DEBUG_F("[Memory] ✓ Scratch buffer: %d bytes (%.1f KB)\n", scratchBufferSize, scratchBufferSize / 1024.0);
    }
    
    size_t totalAllocated = imageBufferSize + (fullImageBufferSize * 2) + scaledBufferSize + (scratchBuffer ? scratchBufferSize : 0);
    LOG_DEBUG_F("[Memory] ✓ All buffers allocated: %d bytes total (%.1f MB)\n", 
                 totalAllocated, totalAllocated / (1024.0 * 1024.0));
    LOG_DEBUG_F("[Memory] Free PSRAM remaining: %d bytes (%.1f MB)\n", 
                 ESP.getFreePsram(), ESP.getFreePsram() / (1024.0 * 1024.0));
    LOG_DEBUG_F("PSRAM pre-allocation complete - Free PSRAM remaining: %d bytes\n", ESP.getFreePsram());
    
    // Reset watchdog after large memory allocations
    systemMonitor.forceResetWatchdog();
    
    // NOW initialize display - it should have plenty of contiguous PSRAM remaining
    if (!displayManager.begin()) {
        LOG_CRITICAL("CRITICAL: Display initialization failed!");
        LOG_CRITICAL_F("Free PSRAM at failure: %d bytes\n", ESP.getFreePsram());
        crashLogger.saveBeforeReboot();
        delay(100);
        ESP.restart();
    }
    
    // Verify display dimensions match our configuration
    if (displayManager.getWidth() != w || displayManager.getHeight() != h) {
        LOG_WARNING_F("WARNING: Display dimensions mismatch! Config: %dx%d, Actual: %dx%d\n",
                     w, h, displayManager.getWidth(), displayManager.getHeight());
        LOG_WARNING("This should not happen - check displays_config.h");
    } else {
        LOG_DEBUG_F("Display dimensions verified: %dx%d\n", w, h);
    }
    
    // Reset watchdog after display initialization (heavy operation)
    systemMonitor.forceResetWatchdog();
    
    // Setup debug functions for other modules
    wifiManager.setDebugFunctions(debugPrint, debugPrintf, &firstImageLoaded);
    ppaAccelerator.setDebugFunctions(debugPrint, debugPrintf);
    mqttManager.setDebugFunctions(debugPrint, debugPrintf, &firstImageLoaded);
    
    // Check if WiFi setup is needed before showing startup messages
    bool needsWiFiSetup = !configStorage.isWiFiProvisioned();
    
    if (!needsWiFiSetup) {
        // Show simplified startup banner centered on screen
        // Set starting position lower to avoid cutoff at top
        displayManager.setDebugY(150);  // Start at 150px from top (centered vertically)
        
        debugPrint("ESP32 AllSky Display", COLOR_CYAN);
        debugPrint(" ", COLOR_WHITE);  // Spacing
        
        debugPrint("Initializing hardware...", COLOR_YELLOW);
    }
    
    // Buffers already allocated before display init - just initialize PPA hardware acceleration
    ppaAccelerator.begin(w, h);
    
    if (!needsWiFiSetup) {
        // Show hardware initialization result
        debugPrint("Hardware Ready!", COLOR_GREEN);
        debugPrint(" ", COLOR_WHITE);  // Group spacing
    }
    
    // Initialize brightness control
    displayManager.initBrightness();
    
    // Check if WiFi is provisioned (first boot or after reset)
    // Check if WiFi setup is needed
    if (!configStorage.isWiFiProvisioned()) {
        // Enter non-blocking WiFi setup mode
        wifiSetupMode = true;
        wifiSetupStartTime = millis();
        
        // Clear screen and show WiFi setup instructions
        displayManager.clearScreen();
        
        // Reset watchdog before captive portal operations
        systemMonitor.forceResetWatchdog();
        
        // Start captive portal for WiFi configuration
        if (captivePortal.begin("AllSky-Setup")) {
            // Reset watchdog before QR code display
            systemMonitor.forceResetWatchdog();
            
            // Display QR code first (at top/center) and get text start position
            int16_t textY = displayWiFiQRCode();
            
            // Reset watchdog after QR code display
            systemMonitor.forceResetWatchdog();
            
            // Reset debug Y position to place text below QR code
            displayManager.setDebugY(textY);
            
            // Disable auto-scroll to prevent QR code from being cleared
            displayManager.setDisableAutoScroll(true);
            
            // Show compact centered text instructions below QR code
            debugPrint(" ", COLOR_WHITE);  // Small space
            debugPrint("WiFi Setup Required", COLOR_YELLOW);
            debugPrint(" ", COLOR_WHITE);
            debugPrint("Scan QR or Connect:", COLOR_CYAN);
            debugPrint("AllSky-Setup", COLOR_WHITE);
            debugPrint(" ", COLOR_WHITE);
            debugPrint("Open: 192.168.4.1", COLOR_GREEN);
            debugPrint(" ", COLOR_WHITE);
            debugPrint("Select WiFi & Connect", COLOR_CYAN);
            
            Serial.println("✓ Captive portal started - waiting for WiFi configuration in loop()");
        } else {
            debugPrint("ERROR: Failed to start captive portal", COLOR_RED);
            wifiSetupMode = false;  // Disable setup mode if portal failed
            // Re-enable auto-scroll
            displayManager.setDisableAutoScroll(false);
        }
    } else {
        // WiFi is already configured - connect normally
        // Initialize WiFi manager
        if (!wifiManager.begin()) {
            debugPrint("ERROR: WiFi initialization failed!", COLOR_RED);
            debugPrint("Please configure WiFi in config.cpp", COLOR_YELLOW);
        } else {
            // Try to connect to WiFi with watchdog protection
            systemMonitor.forceResetWatchdog();
            
            // CRITICAL: Allow WiFi hardware to fully initialize before connection attempt
            // Without this delay, MAC address may show as 00:00:00:00:00:00 and connection fails
            delay(500);
            
            wifiManager.connectToWiFi();
            systemMonitor.forceResetWatchdog();  // Reset after WiFi connect attempt
            
            // Check if WiFi connection failed
            if (!wifiManager.isConnected()) {
                systemMonitor.forceResetWatchdog();
            }
        }
    }
    
    // Initialize MQTT manager
    mqttManager.begin();
    
    // Initialize OTA manager
    otaManager.begin();
    otaManager.setDebugFunction(debugPrint);
    
    // Start web configuration server if WiFi is connected
    if (wifiManager.isConnected()) {
        webConfig.begin(8080);
        wifiManager.initOTA();
        
        if (!needsWiFiSetup) {
            debugPrint("System Ready!", COLOR_GREEN);
            debugPrint(" ", COLOR_WHITE);  // Group spacing
            
            // Pause to allow user to read boot messages
            delay(2000);
        }
    }
    
    // Load cycling configuration after all modules are initialized
    loadCyclingConfiguration();
    
    // Load transform settings for the current image
    updateCurrentImageTransformSettings();
    
    // Initialize touch controller
    initializeTouchController();
    
    // Ensure image processing flag is reset (in case of crash/reboot during processing)
    imageProcessing = false;
    
    // =============================================================================
    // INITIALIZE ASYNC DOWNLOAD TASK
    // =============================================================================
    // Create synchronization primitives for async image downloads
    imageBufferMutex = xSemaphoreCreateMutex();
    if (!imageBufferMutex) {
        Serial.println("ERROR: Failed to create image buffer mutex");
    }
    
    imageReadyQueue = xQueueCreate(1, sizeof(uint8_t*));
    if (!imageReadyQueue) {
        Serial.println("ERROR: Failed to create image ready queue");
    }
    
    // Create download task on Core 0 (network/WiFi core)
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        downloadTask,                    // Task function
        "ImageDownloader",               // Task name
        DOWNLOAD_TASK_STACK_SIZE,        // Stack size
        NULL,                            // Task parameters
        DOWNLOAD_TASK_PRIORITY,          // Task priority
        &downloadTaskHandle,             // Task handle
        0                                // Pin to Core 0 (Network Core)
    );
    
    if (taskCreated != pdPASS) {
        Serial.println("ERROR: Failed to create download task");
    } else {
        Serial.println("✓ Async download task created on Core 0");
    }
    
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
    
    // FLICKER FIX: Skip clearing on image updates to avoid black flash
    // Only clear on first image load, subsequent updates render directly without clearing
    systemMonitor.forceResetWatchdog();
    
    if (!hasSeenFirstImage) {
        // First image ONLY: clear entire screen once
        displayManager.clearScreen();
        hasSeenFirstImage = true;
        systemMonitor.forceResetWatchdog();
    }
    // NOTE: Subsequent images skip clearing entirely - the buffer swap ensures
    // we only update when the new image is 100% ready, so no intermediate states
    // exist that would require clearing. This eliminates the black flash!
    
    // Reset watchdog before rendering operations
    systemMonitor.forceResetWatchdog();
    
    if (scaleX == 1.0 && scaleY == 1.0 && rotationAngle == 0.0) {
        // No scaling or rotation needed, direct copy
        displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        
        // Flush to display
        Arduino_DSI_Display* gfx = displayManager.getGFX();
        if (gfx) gfx->flush();
        
        systemMonitor.forceResetWatchdog();
        
        // Update tracking variables for next transition
        prevImageX = finalX;
        prevImageY = finalY;
        prevImageWidth = fullImageWidth;
        prevImageHeight = fullImageHeight;
    } else {
        // Try hardware acceleration first if available
        size_t scaledImageSize = scaledWidth * scaledHeight * 2;
        
        Serial.printf("[Render] Image: %dx%d -> Scaled: %dx%d (rot:%.0f) = %d bytes vs buffer %d bytes\n",
                     fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, 
                     rotationAngle, scaledImageSize, scaledBufferSize);
        Serial.printf("[Render] PPA available: %s, Size check: %s\n", 
                     ppaAccelerator.isAvailable() ? "YES" : "NO",
                     scaledImageSize <= scaledBufferSize ? "PASS" : "FAIL");
        
        if (ppaAccelerator.isAvailable() && scaledImageSize <= scaledBufferSize) {
            systemMonitor.forceResetWatchdog();
            
            unsigned long hwStart = millis();
            
            // Pause display during heavy PPA operations to prevent LCD underrun
            displayManager.pauseDisplay();
            
            Serial.printf("[PPA] Attempting hardware scale+rotate: %dx%d -> %dx%d (%.0f°)\n",
                         fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, rotationAngle);
            
            if (ppaAccelerator.scaleRotateImage(fullImageBuffer, fullImageWidth, fullImageHeight,
                                              scaledBuffer, scaledWidth, scaledHeight, rotationAngle)) {
                // Resume display after heavy operation
                displayManager.resumeDisplay();
                
                systemMonitor.forceResetWatchdog();
                
                unsigned long hwTime = millis() - hwStart;
                Serial.printf("[PPA] ✓ Hardware acceleration successful in %lu ms\n", hwTime);
                debugPrintf(COLOR_GREEN, "PPA hardware render: %lu ms", hwTime);
                
                // Draw the hardware-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
                
                // Flush to display
                Arduino_DSI_Display* gfx = displayManager.getGFX();
                if (gfx) gfx->flush();
                
                systemMonitor.forceResetWatchdog();
                
                // Update tracking variables for next transition
                prevImageX = finalX;
                prevImageY = finalY;
                prevImageWidth = scaledWidth;
                prevImageHeight = scaledHeight;
                return;
            } else {
                Serial.println("[PPA] ✗ Hardware acceleration failed, falling back to software");
                debugPrint("DEBUG: PPA scale+rotate failed, falling back to software", COLOR_YELLOW);
                displayManager.resumeDisplay();  // Resume display if PPA failed
                systemMonitor.forceResetWatchdog();
            }
        } else {
            if (!ppaAccelerator.isAvailable()) {
                Serial.println("[PPA] Hardware acceleration not available");
            } else {
                Serial.printf("[PPA] Scaled image too large: %d > %d bytes\n", scaledImageSize, scaledBufferSize);
            }
        }
        
        // Software fallback using ImageUtils bilinear scaling
        Serial.println("[Render] Using software transformation fallback");
        debugPrint("DEBUG: Software scaling (bilinear)", COLOR_YELLOW);
        systemMonitor.forceResetWatchdog();
        
        if (scaledImageSize <= scaledBufferSize) {
            // Use ImageUtils for software scaling
            unsigned long swStart = millis();
            
            if (ImageUtils::softwareTransform(
                fullImageBuffer, fullImageWidth, fullImageHeight,
                scaledBuffer, scaledWidth, scaledHeight,
                (int)rotationAngle
            )) {
                unsigned long swTime = millis() - swStart;
                Serial.printf("[Render] ✓ Software scaling complete in %lu ms\n", swTime);
                debugPrintf(COLOR_GREEN, "SW render: %lu ms", swTime);
                
                // Draw the software-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
                
                // Flush to display
                Arduino_DSI_Display* gfx = displayManager.getGFX();
                if (gfx) gfx->flush();
                
                systemMonitor.forceResetWatchdog();
                
                // Update tracking variables for next transition
                prevImageX = finalX;
                prevImageY = finalY;
                prevImageWidth = scaledWidth;
                prevImageHeight = scaledHeight;
                return;
            } else {
                Serial.println("[Render] ✗ Software scaling failed");
                debugPrint("ERROR: Software scaling failed", COLOR_RED);
            }
        }
        
        // Ultimate fallback: draw original unscaled image
        Serial.println("[Render] Drawing original unscaled image");
        debugPrint("WARNING: Showing unscaled image", COLOR_YELLOW);
        displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        
        // Flush to display
        Arduino_DSI_Display* gfx = displayManager.getGFX();
        if (gfx) gfx->flush();
        
        systemMonitor.forceResetWatchdog();
        
        // Update tracking variables for next transition
        prevImageX = finalX;
        prevImageY = finalY;
        prevImageWidth = fullImageWidth;
        prevImageHeight = fullImageHeight;
    }
    
    // Final watchdog reset before function exit
    systemMonitor.forceResetWatchdog();
}

void downloadAndDisplayImage() {
    // Check if already processing an image (mutex protection)
    if (imageProcessing) {
        Serial.println("WARNING: Image processing already in progress - skipping concurrent call");
        return;
    }
    
    // Set processing flag (simple mutex)
    imageProcessing = true;
    
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
        imageProcessing = false;  // Clear mutex before return
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

    Serial.flush();
    
    // Reset watchdog before HTTP operations
    systemMonitor.forceResetWatchdog();
    
    Serial.println("[Image] ===== Starting Image Download =====");
    Serial.printf("[Image] URL: %s\n", imageURL);
    Serial.printf("[Image] Buffer size: %d bytes\n", imageBufferSize);
    Serial.printf("[Image] Free heap: %d bytes, Free PSRAM: %d bytes\n", ESP.getFreeHeap(), ESP.getFreePsram());
    
    HTTPClient http;
    
    // Reset watchdog before potentially blocking http.begin() call
    systemMonitor.forceResetWatchdog();
    
    unsigned long httpBeginStart = millis();
    Serial.println("[Image] Initializing HTTP client...");
    
    // Enhanced HTTP begin operation with task-based timeout protection
    bool httpBeginSuccess = false;
    
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
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    if (!beginResult) {
        debugPrintf(COLOR_RED, "ERROR: HTTP begin failed after %lu ms", httpBeginTime);
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    // Enable redirect following for URLs like picsum.photos
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    
    // Set enhanced timeouts to prevent any blocking
    http.setTimeout(HTTP_REQUEST_TIMEOUT);      // Use config value
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT); // Use config value
    
    // Add User-Agent and other headers for better compatibility
    http.addHeader("User-Agent", "ESP32-AllSky/1.0");
    http.addHeader("Connection", "close");
    http.addHeader("Cache-Control", "no-cache");
    
    // Reset watchdog before GET request
    systemMonitor.forceResetWatchdog();
    
    unsigned long downloadStart = millis();
    
    // Enhanced GET request with timeout monitoring
    int httpCode = -1;
    unsigned long getRequestStart = millis();
    
    // Reset watchdog right before GET with frequent monitoring
    systemMonitor.forceResetWatchdog();
    
    // Start GET request with enhanced error handling
    Serial.println("[Image] Sending HTTP GET request...");
    try {
        httpCode = http.GET();
        Serial.printf("[Image] HTTP response code: %d\n", httpCode);
        String finalURL = http.getLocation();
        if (finalURL.length() > 0 && finalURL != imageURL) {
            Serial.printf("[Image] Redirected to: %s\n", finalURL.c_str());
        }
    } catch (...) {
        Serial.println("[Image] ✗ EXCEPTION during HTTP GET!");
        debugPrint("ERROR: Exception during HTTP GET", COLOR_RED);
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    unsigned long getRequestTime = millis() - getRequestStart;
    
    // Immediate timeout check with config value
    if (getRequestTime >= HTTP_REQUEST_TIMEOUT) {
        debugPrintf(COLOR_RED, "ERROR: HTTP GET timed out after %lu ms", getRequestTime);
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    // Reset watchdog immediately after GET
    systemMonitor.forceResetWatchdog();
    
    // Enhanced error handling for different HTTP codes
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Image] ✗ HTTP request failed with code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_NOT_FOUND) {
            Serial.println("[Image] Error: Image not found (404) - Check URL");
            debugPrint("ERROR: Image not found (404)", COLOR_RED);
        } else if (httpCode == HTTP_CODE_INTERNAL_SERVER_ERROR) {
            Serial.println("[Image] Error: Server error (500) - Try again later");
            debugPrint("ERROR: Server error (500)", COLOR_RED);
        } else if (httpCode < 0) {
            Serial.print("[Image] Connection error: ");
            if (httpCode == -1) Serial.println("HTTPC_ERROR_CONNECTION_FAILED");
            else if (httpCode == -2) Serial.println("HTTPC_ERROR_SEND_HEADER_FAILED");
            else if (httpCode == -3) Serial.println("HTTPC_ERROR_SEND_PAYLOAD_FAILED");
            else if (httpCode == -4) Serial.println("HTTPC_ERROR_NOT_CONNECTED");
            else if (httpCode == -5) Serial.println("HTTPC_ERROR_CONNECTION_LOST");
            else if (httpCode == -6) Serial.println("HTTPC_ERROR_NO_STREAM");
            else if (httpCode == -7) Serial.println("HTTPC_ERROR_NO_HTTP_SERVER");
            else if (httpCode == -8) Serial.println("HTTPC_ERROR_TOO_LESS_RAM");
            else if (httpCode == -11) Serial.println("HTTPC_ERROR_READ_TIMEOUT");
            else Serial.printf("Unknown error code: %d\n", httpCode);
            debugPrintf(COLOR_RED, "ERROR: Connection error: %d", httpCode);
        } else {
            Serial.printf("[Image] HTTP error code: %d\n", httpCode);
            debugPrintf(COLOR_RED, "ERROR: HTTP error: %d", httpCode);
        }
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.println("[Image] ✓ HTTP request successful");
    
    // Reset watchdog before processing response
    systemMonitor.forceResetWatchdog();
    
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    
    Serial.printf("[Image] Content-Length: %d bytes\n", size);
    if (cyclingEnabled && imageSourceCount > 1) {
        Serial.printf("[Image] Source: Image %d/%d - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL);
        debugPrintf(COLOR_WHITE, "Image %d/%d: %d bytes", currentImageIndex + 1, imageSourceCount, size);
    } else {
        Serial.printf("[Image] Source: %s\n", currentImageURL);
        debugPrintf(COLOR_WHITE, "Image: %d bytes", size);
    }
    
    // Validate size before proceeding
    if (size <= 0) {
        LOG_WARNING("[ImageDownload] Content-Length missing or zero - aborting download");
        Serial.println("[Image] ⚠ Content-Length missing or zero - will download until stream ends");
        Serial.printf("[Image] Buffer capacity: %d bytes\n", imageBufferSize);
        debugPrintf(COLOR_RED, "Invalid size: %d bytes", size);
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    if (size >= imageBufferSize) {
        LOG_ERROR_F("[ImageDownload] Image too large: %d bytes exceeds buffer capacity of %d bytes\n", size, imageBufferSize);
        Serial.printf("[Image] ✗ Image too large! %d bytes exceeds buffer %d bytes\n", size, imageBufferSize);
        debugPrintf(COLOR_RED, "Invalid size: %d bytes", size);
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.printf("[Image] ✓ Size OK: %d/%d bytes (%.1f%% of buffer)\n", size, imageBufferSize, (size * 100.0) / imageBufferSize);
    
    Serial.println("[Image] Starting download stream...");
    Serial.printf("[Image] Download config: 1KB chunks, 50ms watchdog, 5s timeout\n");
    // Show downloading message on display for first image only
    if (!firstImageLoaded) {
        debugPrint("Downloading Image...", COLOR_YELLOW);
    }
    LOG_DEBUG("Downloading image data...");
    
    size_t bytesRead = 0;
    uint8_t* buffer = imageBuffer;
    unsigned long readStart = millis();
    
    // Use configuration values for better consistency
    const size_t ULTRA_CHUNK_SIZE = 1024;  // 1KB chunks for good performance

    const unsigned long NO_DATA_TIMEOUT = 5000;  // 5 seconds with no data before giving up (increased for slow connections)
    
    unsigned long downloadStartTime = millis();
    unsigned long lastProgressTime = millis();
    unsigned long lastWatchdogReset = millis();
    unsigned long lastDataTime = millis();  // Track when we last received data
    
    Serial.printf("[Image] Download started at %lu ms uptime\n", downloadStartTime);
    
    while (http.connected() && bytesRead < size) {
        // Ultra-frequent watchdog reset - every 50ms
        if (millis() - lastWatchdogReset > DOWNLOAD_WATCHDOG_INTERVAL) {
            systemMonitor.forceResetWatchdog();
            lastWatchdogReset = millis();
        }
        
        // Check for overall download timeout
        if (millis() - downloadStartTime > TOTAL_DOWNLOAD_TIMEOUT) {
            Serial.printf("[Image] ✗ Total download timeout! Exceeded %lu ms limit\n", TOTAL_DOWNLOAD_TIMEOUT);
            Serial.printf("[Image] Downloaded %d/%d bytes (%.1f%%) before timeout\n", 
                         bytesRead, size, (bytesRead * 100.0) / size);
            debugPrint("ERROR: Download timeout - aborting", COLOR_RED);
            break;
        }
        
        // Check if connection is still alive
        if (!http.connected()) {
            Serial.println("[Image] ✗ HTTP connection lost during download!");
            Serial.printf("[Image] Downloaded %d/%d bytes (%.1f%%) before disconnection\n", 
                         bytesRead, size, (bytesRead * 100.0) / size);
            Serial.printf("[Image] Connection duration: %lu ms\n", millis() - downloadStartTime);
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
                Serial.printf("[Image] ⚠ Slow chunk: %lu ms (limit %lu ms)\n", chunkTime, DOWNLOAD_CHUNK_TIMEOUT);
                debugPrintf(COLOR_YELLOW, "WARNING: Chunk read took %lu ms", chunkTime);
                // If chunks are consistently slow, abort
                if (chunkTime > DOWNLOAD_CHUNK_TIMEOUT * 2) {  // More aggressive - 2x timeout
                    Serial.printf("[Image] ✗ Chunk timeout exceeded! %lu ms > %lu ms\n", 
                                 chunkTime, DOWNLOAD_CHUNK_TIMEOUT * 2);
                    Serial.printf("[Image] Aborting slow download at %d/%d bytes\n", bytesRead, size);
                    debugPrint("ERROR: Chunk timeout - aborting", COLOR_RED);
                    break;
                }
            }
            
            if (read > 0) {
                bytesRead += read;
                lastDataTime = millis();  // Update last data time on successful read
                
                // Show progress and reset watchdog
                if (millis() - lastProgressTime > 1000) {  // Every 1 second
                    float progress = (bytesRead * 100.0) / size;
                    float speed = (bytesRead * 1000.0) / (millis() - downloadStartTime); // bytes/sec
                    Serial.printf("[Image] Progress: %.1f%% (%d/%d bytes) @ %.1f KB/s\n", 
                                 progress, bytesRead, size, speed / 1024.0);
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
                Serial.printf("[Image] ✗ Download stalled! No data for %lu ms\n", millis() - lastDataTime);
                Serial.printf("[Image] Downloaded %d/%d bytes before stall (%.1f%%)\n", 
                             bytesRead, size, (bytesRead * 100.0) / size);
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
    
    // Close HTTP connection immediately (also cleans up WiFiClient)
    http.end();
    
    systemMonitor.forceResetWatchdog();
    
    float avgSpeed = readTime > 0 ? (bytesRead * 1000.0) / readTime : 0; // bytes/sec
    Serial.printf("[Image] ✓ Download complete: %d bytes in %lu ms (%.1f KB/s avg)\n", 
                 bytesRead, readTime, avgSpeed / 1024.0);
    Serial.printf("DEBUG: Download complete - Read %d bytes (expected %d)\n", bytesRead, size);
    
    // Validate download completeness
    if (bytesRead < size) {
        float percentComplete = (bytesRead * 100.0) / size;
        Serial.printf("[Image] ✗ Incomplete download! Got %d/%d bytes (%.1f%%)\n", bytesRead, size, percentComplete);
        Serial.printf("[Image] Missing %d bytes\n", size - bytesRead);
        debugPrintf(COLOR_RED, "Incomplete download: %d/%d bytes", bytesRead, size);
        Serial.printf("ERROR: Incomplete download: %d/%d bytes\n", bytesRead, size);
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    // Additional validation: ensure we have enough data for JPEG processing
    if (bytesRead < 1024) {  // Minimum reasonable JPEG size
        Serial.printf("[Image] ✗ Image too small! %d bytes (minimum 1024 for valid JPEG)\n", bytesRead);
        debugPrintf(COLOR_RED, "Downloaded data too small: %d bytes", bytesRead);
        Serial.printf("ERROR: Downloaded data too small: %d bytes (minimum 1024)\n", bytesRead);
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.println("[Image] ✓ Download size validated");
    
    Serial.println("DEBUG: Size validation passed");
    
    // Reset watchdog before JPEG processing
    systemMonitor.forceResetWatchdog();
    
    // Check JPEG header first
    if (bytesRead < 10) {
        Serial.printf("[Image] ✗ Data too small for header validation: %d bytes (need 10)\n", bytesRead);
        debugPrintf(COLOR_RED, "ERROR: Downloaded data too small: %d bytes", bytesRead);
        Serial.printf("ERROR: Downloaded data too small for header check: %d bytes\n", bytesRead);
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.println("[Image] Validating image format...");
    Serial.printf("[Image] Header bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                 imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3],
                 imageBuffer[4], imageBuffer[5], imageBuffer[6], imageBuffer[7]);
    Serial.printf("DEBUG: Checking image format (first 4 bytes: %02X %02X %02X %02X)...\n",
                 imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
    
    // Check for PNG magic numbers first
    if (imageBuffer[0] == 0x89 && imageBuffer[1] == 0x50 && imageBuffer[2] == 0x4E && imageBuffer[3] == 0x47) {
        Serial.println("[Image] ✗ PNG format detected (0x89504E47)");
        debugPrint("ERROR: PNG format detected - not supported (JPEG only)", COLOR_RED);
        Serial.println("ERROR: PNG format detected - not supported (JPEG only)");
        Serial.println("This device only supports JPEG images. Please use a JPEG format image URL.");
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.println("[Image] Not PNG, checking for JPEG...");
    Serial.println("DEBUG: Not PNG format, checking for JPEG...");
    
    // Check for JPEG magic numbers
    if (imageBuffer[0] != 0xFF || imageBuffer[1] != 0xD8) {
        Serial.printf("[Image] ✗ Invalid JPEG header: 0x%02X%02X (expected 0xFFD8)\n", imageBuffer[0], imageBuffer[1]);
        debugPrintf(COLOR_RED, "ERROR: Invalid JPEG header: 0x%02X%02X (expected 0xFFD8)", imageBuffer[0], imageBuffer[1]);
        Serial.printf("ERROR: Invalid JPEG header: 0x%02X%02X (expected 0xFFD8)\n", imageBuffer[0], imageBuffer[1]);
        imageProcessing = false;  // Clear mutex before return
        return;
    }
    
    Serial.println("[Image] ✓ Valid JPEG header (0xFFD8)");
    
    Serial.println("[Image] Opening JPEG decoder...");
    Serial.printf("[Image] JPEG data: %d bytes in RAM\n", bytesRead);
    Serial.printf("DEBUG: Opening JPEG in RAM (%d bytes)...\n", bytesRead);
    if (jpeg.openRAM(imageBuffer, bytesRead, JPEGDraw)) {
        Serial.println("[Image] ✓ JPEG decoder opened");
        Serial.printf("[Image] Dimensions: %dx%d pixels\n", jpeg.getWidth(), jpeg.getHeight());
        Serial.printf("DEBUG: JPEG opened successfully - %dx%d\n", jpeg.getWidth(), jpeg.getHeight());
        pendingImageWidth = jpeg.getWidth();
        pendingImageHeight = jpeg.getHeight();
        
        // Check if image fits in our pending buffer
        size_t requiredSize = pendingImageWidth * pendingImageHeight * 2;
        Serial.printf("[Image] Buffer check: %d bytes required, %d bytes available\n", requiredSize, fullImageBufferSize);
        Serial.printf("DEBUG: Image size check - Required: %d bytes, Available: %d bytes\n", requiredSize, fullImageBufferSize);
        
        if (requiredSize <= fullImageBufferSize) {
            Serial.println("DEBUG: Image fits in buffer, proceeding with decode...");
            // Clear the PENDING image buffer
            memset(pendingFullImageBuffer, 0, pendingImageWidth * pendingImageHeight * sizeof(uint16_t));
            
            // Decode the full image into PENDING buffer with watchdog protection
            systemMonitor.forceResetWatchdog();
            
            unsigned long decodeStart = millis();
            
            // JPEG decode with timeout monitoring

            unsigned long decodeStartTime = millis();
            
            // Pause display during decode to prevent LCD underrun from memory contention
            displayManager.pauseDisplay();
            
            Serial.println("[Image] Decoding JPEG to RGB565...");
            if (jpeg.decode(0, 0, 0)) {
                // Resume display after decode completes
                displayManager.resumeDisplay();
                
                unsigned long decodeTime = millis() - decodeStart;
                Serial.printf("[Image] ✓ Decode complete in %lu ms\n", decodeTime);
                Serial.printf("[Image] Decoded %dx%d pixels (%d bytes RGB565)\n", 
                             pendingImageWidth, pendingImageHeight, 
                             pendingImageWidth * pendingImageHeight * 2);
                
                // Reset watchdog after decode
                systemMonitor.forceResetWatchdog();
                
                // Mark image as ready to display (but don't display yet - let loop handle it)
                imageReadyToDisplay = true;
                if (cyclingEnabled && imageSourceCount > 1) {
                    Serial.printf("[Image] Image %d/%d ready to display - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL);
                    debugPrintf(COLOR_GREEN, "Image %d/%d ready", currentImageIndex + 1, imageSourceCount);
                } else {
                    Serial.printf("[Image] Image ready to display - %s\n", currentImageURL);
                    debugPrintf(COLOR_GREEN, "Image ready");
                }
                Serial.println("Image fully decoded and ready for display");
                
                // Mark first image as loaded (only once) - happens before actual display
                if (!firstImageLoaded) {
                    firstImageLoaded = true;
                    displayManager.setFirstImageLoaded(true);
                    Serial.println("First image loaded successfully - switching to image mode");
                    debugPrint("DEBUG: First image loaded flag set", COLOR_GREEN);
                } else {
                    Serial.println("Image prepared successfully - ready for seamless display");
                }
            } else {
                // Resume display even on error
                displayManager.resumeDisplay();
                
                unsigned long decodeAttemptTime = millis() - decodeStart;
                Serial.printf("[Image] ✗ JPEG decode failed after %lu ms\n", decodeAttemptTime);
                Serial.printf("[Image] Image was: %dx%d pixels, %d bytes\n", 
                             pendingImageWidth, pendingImageHeight, bytesRead);
                Serial.println("[Image] Possible causes:");
                Serial.println("  - Corrupted JPEG data");
                Serial.println("  - Unsupported JPEG format/encoding");
                Serial.println("  - Progressive JPEG (not supported)");
                Serial.println("  - Memory allocation failure during decode");
                
                debugPrint("ERROR: JPEG decode() function failed!", COLOR_RED);
                Serial.println("ERROR: JPEG decode() function failed!");
            }
            
            jpeg.close();
            systemMonitor.forceResetWatchdog();
        } else {
            // Image too large for buffer
            Serial.printf("[Image] ✗ Image exceeds buffer capacity!\n");
            Serial.printf("[Image] Required: %d bytes (%dx%d @ 2 bytes/pixel)\n", 
                         requiredSize, pendingImageWidth, pendingImageHeight);
            Serial.printf("[Image] Available: %d bytes (max 512x512 pixels)\n", fullImageBufferSize);
            Serial.printf("[Image] Overflow: %d bytes\n", requiredSize - fullImageBufferSize);
            debugPrintf(COLOR_RED, "ERROR: Image too large for buffer! Required: %d, Available: %d", 
                       requiredSize, fullImageBufferSize);
            Serial.printf("ERROR: Image too large for buffer! Required: %d bytes, Available: %d bytes\n", 
                         requiredSize, fullImageBufferSize);
            jpeg.close();
        }
    } else {
        Serial.printf("[Image] ✗ JPEG decoder failed to open image\n");
        Serial.printf("[Image] Data size: %d bytes\n", bytesRead);
        Serial.printf("[Image] Header: %02X %02X %02X %02X\n", 
                     imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
        Serial.println("[Image] Possible causes:");
        Serial.println("  - Corrupted download");
        Serial.println("  - Incomplete file");
        Serial.println("  - Not a valid JPEG despite header");
        Serial.println("  - Memory allocation failure");
        debugPrint("ERROR: JPEG openRAM() failed!", COLOR_RED);
        debugPrintf(COLOR_RED, "Downloaded %d bytes, buffer size: %d", bytesRead, imageBufferSize);
        Serial.printf("ERROR: JPEG openRAM() failed! Downloaded %d bytes, buffer size: %d\n", bytesRead, imageBufferSize);
        Serial.printf("First 16 bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                     imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3],
                     imageBuffer[4], imageBuffer[5], imageBuffer[6], imageBuffer[7],
                     imageBuffer[8], imageBuffer[9], imageBuffer[10], imageBuffer[11],
                     imageBuffer[12], imageBuffer[13], imageBuffer[14], imageBuffer[15]);
    }
    
    // Final watchdog reset and cleanup
    systemMonitor.forceResetWatchdog();
    debugPrintf(COLOR_WHITE, "Free heap: %d bytes", systemMonitor.getCurrentFreeHeap());
    Serial.printf("[Image] Download cycle completed for image %d/%d\n", currentImageIndex + 1, imageSourceCount);
    debugPrint("Download cycle completed", COLOR_GREEN);
    
    // Clear processing flag (release mutex)
    imageProcessing = false;
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
    
    // Also load the transform settings for the current image
    updateCurrentImageTransformSettings();
}

// Load transform settings for the current image from configuration storage
void updateCurrentImageTransformSettings() {
    if (imageSourceCount > 0) {
        int index = currentImageIndex;
        scaleX = configStorage.getImageScaleX(index);
        scaleY = configStorage.getImageScaleY(index);
        offsetX = configStorage.getImageOffsetX(index);
        offsetY = configStorage.getImageOffsetY(index);
        rotationAngle = configStorage.getImageRotation(index);
        
        Serial.printf("Loaded transform settings for image %d: scale=%.1fx%.1f, offset=%d,%d, rotation=%.0f°\n",
                     index, scaleX, scaleY, offsetX, offsetY, rotationAngle);
    }
}

// Advance to next image in cycling sequence
void advanceToNextImage() {
    // Allow manual advancing even if cycling is disabled
    if (imageSourceCount <= 1) {
        Serial.println("Cannot advance: only 1 image source configured");
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
    
    // Update transform settings for the new image
    updateCurrentImageTransformSettings();
    
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

// =============================================================================
// ASYNC DOWNLOAD TASK (FreeRTOS Task on Core 0)
// =============================================================================
// This task runs on Core 0 to handle image downloads asynchronously,
// preventing the main UI loop from freezing during network operations.
void downloadTask(void* params) {
    const TickType_t xDelay = pdMS_TO_TICKS(100);
    
    // Subscribe this task to the watchdog
    esp_task_wdt_add(NULL);  // NULL = current task
    Serial.println("[DownloadTask] Task subscribed to watchdog");
    
    for(;;) {
        if (imageDownloadPending) {
            // Reset WDT once at start of heavy operation
            esp_task_wdt_reset();
            
            Serial.println("[DownloadTask] Image download triggered");
            
            // Perform the download operation
            // NOTE: downloadAndDisplayImage currently writes to fullImageBuffer
            // In a future optimization, this should be refactored to write to
            // pendingFullImageBuffer and signal via queue when ready
            downloadAndDisplayImage();
            
            // Clear the pending flag
            imageDownloadPending = false;
            
            Serial.println("[DownloadTask] Download complete, flag cleared");
        }
        
        // Reset watchdog before yielding
        esp_task_wdt_reset();
        
        // Yield to other tasks
        vTaskDelay(xDelay);
    }
}


void loop() {
    // Force watchdog reset at start of each loop iteration
    systemMonitor.forceResetWatchdog();
    
    // =============================================================================
    // WIFI SETUP MODE - NON-BLOCKING CAPTIVE PORTAL HANDLING
    // =============================================================================
    // If in WiFi setup mode, handle captive portal and skip rest of loop
    if (wifiSetupMode) {
        captivePortal.handleClient();
        systemMonitor.forceResetWatchdog();
        
        // Check if WiFi configuration is complete
        if (captivePortal.isConfigured()) {
            displayManager.setDisableAutoScroll(false);  // Re-enable auto-scroll
            debugPrint("WiFi configured successfully!", COLOR_GREEN);
            debugPrint("Restarting device...", COLOR_YELLOW);
            captivePortal.stop();
            delay(2000);
            crashLogger.saveBeforeReboot();
            delay(100);
            ESP.restart();
        }
        
        // Check for timeout (5 minutes)
        if (millis() - wifiSetupStartTime > 300000) {
            Serial.println("WiFi setup timeout - continuing without WiFi");
            debugPrint("Configuration timeout", COLOR_RED);
            debugPrint("Continuing without WiFi...", COLOR_YELLOW);
            captivePortal.stop();
            displayManager.setDisableAutoScroll(false);  // Re-enable auto-scroll
            wifiSetupMode = false;  // Exit setup mode
            delay(2000);
            displayManager.clearScreen();  // Clear setup screen
        }
        
        // Skip rest of loop during WiFi setup
        delay(10);
        return;
    }
    
    // Process background retry tasks (handles network, MQTT, and image download failures)
    taskRetryHandler.process();
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
    
    // Handle OTA updates if WiFi is connected
    if (wifiManager.isConnected()) {
        wifiManager.handleOTA();
        systemMonitor.forceResetWatchdog();
    }
    
    // Handle web configuration server with better error handling
    if (wifiManager.isConnected()) {
        systemMonitor.forceResetWatchdog();  // Reset before web operations
        if (webConfig.isRunning()) {
            unsigned long webStartTime = millis();
            
            // Handle web requests without timeout restrictions that might interrupt connections
            webConfig.handleClient();
            systemMonitor.forceResetWatchdog();  // Reset after web handling
            
            unsigned long webHandleTime = millis() - webStartTime;
            
            // Only warn if it takes excessively long (increased threshold)
            if (webHandleTime > 5000) {
                Serial.printf("WARNING: Web client handling took %lu ms\n", webHandleTime);
            }
        } else {
            // Try to start web server if not running
            Serial.println("DEBUG: Web server not running, attempting to restart...");
            systemMonitor.forceResetWatchdog();  // Reset before webConfig.begin
            if (webConfig.begin(8080)) {
                Serial.printf("Web configuration server restarted at: http://%s:8080\n", WiFi.localIP().toString().c_str());
            } else {
                Serial.println("ERROR: Failed to restart web configuration server");
            }
            systemMonitor.forceResetWatchdog();  // Reset after attempt
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
        webConfig.loopWebSocket();  // Handle WebSocket events
    }
    
    // Periodically save crash logs to NVS (every hour to reduce flash wear)
    // DISABLED: This was causing hourly reboots due to watchdog timeout during NVS write
    // Crash logger already auto-saves on crashes, panics, and intentional reboots
    /* static unsigned long lastNVSSave = 0;
    if (millis() - lastNVSSave > 3600000) {  // 1 hour
        crashLogger.saveToNVS();
        lastNVSSave = millis();
    }
    */
    
    // Check for critical system health issues
    if (!systemMonitor.isSystemHealthy()) {
        Serial.println("CRITICAL: System health compromised, attempting recovery...");
        crashLogger.log("CRITICAL: System health compromised\n");
        systemMonitor.forceResetWatchdog();
        systemMonitor.safeDelay(5000);
        systemMonitor.forceResetWatchdog();
    }
    
    // Process serial commands for image control
    commandInterpreter.processCommands();
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
            // Reset cycle timer to start fresh interval
            lastCycleTime = millis();
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
    // Skip image processing during OTA to prevent interference
    if (!imageProcessing && !imageDownloadPending && !webConfig.isOTAInProgress() && (shouldCycle || currentTime - lastUpdate >= currentUpdateInterval || lastUpdate == 0)) {
        // Pre-download system health check
        if (!wifiManager.isConnected()) {
            Serial.println("WARNING: WiFi disconnected, skipping image download");
            lastUpdate = currentTime; // Update timestamp to prevent immediate retry
            systemMonitor.forceResetWatchdog();
        } else {
            Serial.printf("DEBUG: Triggering async image download (last update: %lu ms ago)\n", 
                         currentTime - lastUpdate);
            
            // Trigger async download on Core 0
            imageDownloadPending = true;
            lastUpdate = currentTime;
            lastImageProcessTime = currentTime;
            
            systemMonitor.forceResetWatchdog();
        }
    }
    
    // Check if new image is ready to display - swap buffers for seamless transition (NO FLICKER!)
    // Skip image rendering during OTA to prevent display interference
    if (imageReadyToDisplay && !webConfig.isOTAInProgress()) {
        imageReadyToDisplay = false;  // Clear the flag immediately
        
        Serial.println("=== SWAPPING IMAGE BUFFERS FOR SEAMLESS DISPLAY ===");
        systemMonitor.forceResetWatchdog();
        
        // Swap the buffers: move pending->active
        uint16_t* tempBuffer = fullImageBuffer;
        fullImageBuffer = pendingFullImageBuffer;
        pendingFullImageBuffer = tempBuffer;
        
        int16_t tempWidth = fullImageWidth;
        fullImageWidth = pendingImageWidth;
        pendingImageWidth = tempWidth;
        
        int16_t tempHeight = fullImageHeight;
        fullImageHeight = pendingImageHeight;
        pendingImageHeight = tempHeight;
        
        Serial.printf("Buffer swap complete: %dx%d image now active\n", fullImageWidth, fullImageHeight);
        systemMonitor.forceResetWatchdog();
        
        // Now render the new image to display (single seamless update, no clearing artifacts)
        if (cyclingEnabled && imageSourceCount > 1) {
            Serial.printf("[Image] Rendering image %d/%d - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL);
            debugPrintf(COLOR_GREEN, "Rendering image %d/%d", currentImageIndex + 1, imageSourceCount);
        } else {
            Serial.printf("[Image] Rendering image - %s\n", currentImageURL);
            debugPrintf(COLOR_GREEN, "Rendering image");
        }
        renderFullImage();
        systemMonitor.forceResetWatchdog();
        
        Serial.println("Image display completed - no flicker!");
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
    // Don't show on display during boot - only log to serial
    // debugPrint("Initializing touch controller...", COLOR_YELLOW);
    LOG_DEBUG("Initializing touch controller...");
    
    try {
        // Initialize I2C interface first
        DEV_I2C_Port i2cPort = DEV_I2C_Init();
        
        // Initialize the GT911 touch controller
        touchHandle = touch_gt911_init(i2cPort);
        
        if (touchHandle != nullptr) {
            touchEnabled = true;
            // Don't show on display - only log
            // debugPrint("Touch controller initialized successfully!", COLOR_GREEN);
            LOG_INFO("GT911 touch controller ready");
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
