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
#include "ha_rest_client.h"  // Home Assistant REST brightness control
#include "moon_ephemeris.h"
#include "moon_sphere.h"
#include "moon_interaction.h"

// Additional required libraries
#include <atomic>
#include <time.h>
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
bool cyclingPausedForEditing = false;  // Pause cycling when user is editing transforms
unsigned long lastEditActivity = 0;     // Track last transform edit time

// Render-reuse cache: when only the per-image offset changes, the scaled+rotated
// buffer from the previous render can be re-drawn without re-running the PPA pass.
uint32_t imageGeneration = 0;   // bumped on every buffer swap (new image); advisory cache key
bool scaledBufferValid = false;
float lastRenderScaleX = 0.0f, lastRenderScaleY = 0.0f, lastRenderRotation = -1.0f;
int lastRenderColorTemp = -1;
uint32_t lastRenderGeneration = 0xFFFFFFFF;

// Download retry: re-attempt a failed download a few times before falling back
// to the normal update interval (faster recovery from transient network errors).
volatile bool imageDownloadFailed = false;
int downloadRetryCount = 0;

// Full image buffer for smooth rendering
uint16_t* fullImageBuffer = nullptr;
size_t fullImageBufferSize = 0;
int16_t fullImageWidth = 0;
int16_t fullImageHeight = 0;

// Pending image buffer (downloaded/decoded but not yet displayed)
uint16_t* pendingFullImageBuffer = nullptr;
int16_t pendingImageWidth = 0;
int16_t pendingImageHeight = 0;
std::atomic<bool> imageReadyToDisplay{false};  // Flag: new image fully prepared and ready to show

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
std::atomic<bool> imageProcessing{false};
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

// Moon drag-to-rotate state. When the currently displayed source is the computed
// moon and the finger travels past MOON_DRAG_THRESHOLD_PX, the gesture becomes a
// rotate (not a tap): serviceMoonDrag() runs an interactive small-render + PPA
// upscale loop until the disc eases back home (snap-back / free-spin).
bool currentSourceIsMoon = false;        // set when the active source is moon://
volatile bool interactiveMoonMode = false;
static const int MOON_DRAG_THRESHOLD_PX = 12;
int moonTouchStartX = 0, moonTouchStartY = 0;
bool moonDragCandidate = false;          // press landed on a moon frame
void serviceMoonDrag();

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
void renderMoonToPendingBuffer();
void loadCyclingConfiguration();
void advanceToNextImage();
String getCurrentImageURL();
void updateCyclingVariables();
void updateCurrentImageTransformSettings();
void downloadTask(void* params);

// Touch function declarations
void initializeTouchController();
void updateTouchState();

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
// Detect log severity from message content using C-string case-insensitive search.
// Avoids allocating a String object and calling toLowerCase() on every call.
static LogSeverity detectSeverity(const char* msg) {
    if (strcasestr(msg, "critical") || strcasestr(msg, "fatal") || strcasestr(msg, "panic")) {
        return LOG_CRITICAL;
    }
    if (strcasestr(msg, "error") || strcasestr(msg, "fail")) {
        return LOG_ERROR;
    }
    if (strcasestr(msg, "warning") || strcasestr(msg, "warn")) {
        return LOG_WARNING;
    }
    if (strcasestr(msg, "info")) {
        return LOG_INFO;
    }
    return LOG_DEBUG;
}

void debugPrint(const char* message, uint16_t color) {
    displayManager.debugPrint(message, color);

    // Log to crash logger (survives reboot)
    crashLogger.log(message);
    crashLogger.log("\n");

    // Detect severity from message content and send to WebSocket console clients
    LogSeverity severity = detectSeverity(message);
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

    // Detect severity from message content and send to WebSocket console clients
    LogSeverity severity = detectSeverity(buffer);
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
    
    LOG_INFO("Loading WiFi QR code...");
    Serial.printf("Display dimensions: %dx%d\n", displayWidth, displayHeight);
    Serial.printf("Scratch buffer: %p\n", scratchBuffer);
    
    // Reset watchdog before system calls
    systemMonitor.forceResetWatchdog();
    
    LOG_DEBUG_F("DEBUG: Free heap before QR: %d bytes\n", systemMonitor.getCurrentFreeHeap());
    LOG_DEBUG_F("DEBUG: Free PSRAM before QR: %d bytes\n", systemMonitor.getCurrentFreePsram());
    
    // Reset watchdog before heavy operation
    systemMonitor.forceResetWatchdog();
    
    // First, open JPEG to get actual dimensions
    if (!jpeg.openRAM((uint8_t*)wifi_qr_code_jpg, wifi_qr_code_jpg_len, JPEGDrawQR)) {
        LOG_ERROR("ERROR: Failed to open QR code JPEG");
        Serial.println("JPEG open failed!");
        return textStartY; // Return default position
    }
    
    Serial.println("JPEG opened successfully");
    
    // Reset watchdog after opening JPEG
    systemMonitor.forceResetWatchdog();
    
    qrCodeWidth = jpeg.getWidth();
    qrCodeHeight = jpeg.getHeight();
    LOG_INFO_F("QR code dimensions: %dx%d\n", qrCodeWidth, qrCodeHeight);
    Serial.printf("QR dimensions: %dx%d\n", qrCodeWidth, qrCodeHeight);
    
    // Check if QR code fits on display
    if (qrCodeWidth > displayWidth || qrCodeHeight > displayHeight) {
        LOG_ERROR("ERROR: QR code too large for display");
        Serial.println("QR too large!");
        jpeg.close();
        return textStartY; // Return default position
    }
    
    // Use scratch buffer instead of allocating new memory
    const size_t qrBufferSize = qrCodeWidth * qrCodeHeight * sizeof(uint16_t);
    const size_t scratchBufferSize = 1024 * 1024;  // 1MB (matches allocation above)
    
    if (!scratchBuffer) {
        LOG_ERROR("ERROR: Scratch buffer not available for QR code");
        Serial.println("Scratch buffer NULL!");
        jpeg.close();
        return textStartY; // Return default position
    }
    
    Serial.printf("QR buffer size: %d, Scratch size: %d\n", qrBufferSize, scratchBufferSize);
    
    if (qrBufferSize > scratchBufferSize) {
        LOG_ERROR_F("ERROR: QR code too large for scratch buffer: %d bytes > %d bytes\n", 
                   qrBufferSize, scratchBufferSize);
        Serial.println("QR too large for scratch!");
        jpeg.close();
        return textStartY;
    }
    
    // Reuse scratch buffer for QR code (no allocation needed!)
    qrCodeBuffer = scratchBuffer;
    
    LOG_INFO_F("Using scratch buffer for QR code: %d bytes\n", qrBufferSize);
    Serial.println("Scratch buffer assigned to QR code buffer");
    
    // Reset watchdog after allocation
    systemMonitor.forceResetWatchdog();
    
    // Clear buffer
    memset(qrCodeBuffer, 0, qrBufferSize);
    Serial.println("QR buffer cleared");
    
    // Reset watchdog before decode
    systemMonitor.forceResetWatchdog();
    
    // Pause display during decode to prevent memory contention
    displayManager.pauseDisplay();
    
    Serial.println("Starting JPEG decode...");
    
    // Decode QR code JPEG
    if (jpeg.decode(0, 0, 0)) {
        LOG_INFO("QR code decoded successfully");
        Serial.println("JPEG decode SUCCESS");
        
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
            
            Serial.println("Starting PPA scaling...");
            
            // Use PPA to scale down
            if (ppaAccelerator.scaleImage(qrCodeBuffer, qrCodeWidth, qrCodeHeight,
                                         scaledQR, scaledWidth, scaledHeight)) {
                LOG_INFO("QR code scaled successfully");
                Serial.println("PPA scaling SUCCESS");
                
                // Reset watchdog after scaling
                systemMonitor.forceResetWatchdog();
                
                // Position at top-center of screen with margin
                int16_t qrX = (displayWidth - scaledWidth) / 2;
                int16_t qrY = 50;  // Smaller margin from top
                
                Serial.printf("Drawing QR at (%d, %d) size %dx%d\n", qrX, qrY, scaledWidth, scaledHeight);
                
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
                
                Serial.println("QR bitmap drawn");
                
                // Flush to ensure QR code is visible immediately
                if (gfx) {
                    gfx->flush();
                    Serial.println("Display flushed");
                }
                
                LOG_INFO_F("Scaled QR code drawn at (%d, %d)\n", qrX, qrY);
                
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
    
    Serial.begin(115200);
    
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

    // Decode the lunar surface texture FIRST, while all PSRAM is free and
    // contiguous. The 2048x1024 JPEG needs a transient ~6 MB contiguous decode
    // buffer (stb_image). If this runs after the 16.9 MB of image buffers below
    // are allocated, the largest free block is too small and the decode fails
    // with "outofmem", falling back to the low-detail procedural placeholder.
    // Decoding here keeps the full-resolution texture (~4 MB resident). Safe to
    // call early: it only touches PSRAM (no display/network), and is idempotent
    // so the later lazy call in renderMoonToPendingBuffer() is a no-op.
    LOG_DEBUG("[Moon] Pre-decoding lunar texture while PSRAM is contiguous...");
    if (moon_sphere_init()) {
        LOG_INFO("[Moon] Lunar texture ready (full resolution)");
    } else {
        LOG_WARNING("[Moon] Lunar texture init failed; placeholder will be used");
    }
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
    if (fullImageBuffer) { heap_caps_free(fullImageBuffer); fullImageBuffer = nullptr; }
    if (pendingFullImageBuffer) { heap_caps_free(pendingFullImageBuffer); pendingFullImageBuffer = nullptr; }
    if (scaledBuffer) { heap_caps_free(scaledBuffer); scaledBuffer = nullptr; }
    
    imageBufferSize = w * h * IMAGE_BUFFER_MULTIPLIER * 2;
    // Apply a floor so large full-disc JPEGs (e.g. GOES-19 1808px ~1.8MB) can be
    // downloaded even on smaller panels whose display-derived size is below it.
    if (imageBufferSize < MIN_DOWNLOAD_BUFFER_SIZE) {
        imageBufferSize = MIN_DOWNLOAD_BUFFER_SIZE;
    }
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
    fullImageBuffer = (uint16_t*)heap_caps_aligned_alloc(64, fullImageBufferSize, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
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
    pendingFullImageBuffer = (uint16_t*)heap_caps_aligned_alloc(64, fullImageBufferSize, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
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
    scaledBuffer = (uint16_t*)heap_caps_aligned_alloc(64, scaledBufferSize, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
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
    // QR code is 594x594 = ~700KB, so allocate 1MB to be safe
    size_t scratchBufferSize = 1024 * 1024;  // 1MB buffer (was 512KB)
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
        
        debugPrint(configStorage.getDeviceName().c_str(), COLOR_CYAN);
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
        
        // Ensure screen clear is flushed to hardware before drawing QR code
        Arduino_DSI_Display* gfx = displayManager.getGFX();
        if (gfx) {
            gfx->flush();
        }
        
        // Reset watchdog before captive portal operations
        systemMonitor.forceResetWatchdog();
        
        // Start captive portal for WiFi configuration (matches QR code SSID)
        if (captivePortal.begin("AllSky-Display-Setup")) {
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
            debugPrint("AllSky-Display-Setup", COLOR_WHITE);
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
    
    // Initialize Home Assistant REST client (runs on Core 0)
    haRestClient.begin();
    
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

// Erase the parts of the previously drawn image that the new draw will NOT
// cover. Without this, changing the per-image X/Y offset (or scale) draws the
// image at the new position but leaves the previous copy on the framebuffer
// (the flicker fix skips a full clearScreen on updates), so a near-full-screen
// image appears not to move. Only the now-uncovered pixels are cleared to the
// background, leaving the overlap untouched, so the flicker-free same-position
// cycling path is unaffected (identical rect -> early return, no fill).
static void eraseUncoveredPrevRegion(Arduino_DSI_Display* gfx,
                                     int16_t nx, int16_t ny, int16_t nw, int16_t nh) {
    if (!gfx) return;
    if (prevImageX == -1 || prevImageWidth == 0 || prevImageHeight == 0) return;  // nothing drawn yet

    const int16_t px = prevImageX, py = prevImageY, pw = prevImageWidth, ph = prevImageHeight;
    if (px == nx && py == ny && pw == nw && ph == nh) return;  // unchanged rect -> no flicker

    const int16_t pl = px, pr = px + pw, pt = py, pb = py + ph;   // previous rect edges
    const int16_t nl = nx, nr = nx + nw, nt = ny, nb = ny + nh;   // new rect edges

    const int16_t ox1 = max(pl, nl), ox2 = min(pr, nr);          // horizontal overlap span
    const int16_t oy1 = max(pt, nt), oy2 = min(pb, nb);          // vertical overlap span

    if (ox1 >= ox2 || oy1 >= oy2) {                              // no overlap: erase whole old rect
        gfx->fillRect(pl, pt, pw, ph, COLOR_BLACK);
        return;
    }

    if (nl > pl) gfx->fillRect(pl, pt, nl - pl, ph, COLOR_BLACK);            // left band (full height)
    if (nr < pr) gfx->fillRect(nr, pt, pr - nr, ph, COLOR_BLACK);            // right band (full height)
    if (nt > pt) gfx->fillRect(ox1, pt, ox2 - ox1, nt - pt, COLOR_BLACK);   // top band (overlap span)
    if (nb < pb) gfx->fillRect(ox1, nb, ox2 - ox1, pb - nb, COLOR_BLACK);   // bottom band (overlap span)
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

    // Clear only the region the previous frame occupied that this frame won't
    // cover. This makes per-image X/Y offset (and scale) changes visibly move
    // the image instead of leaving a stale copy behind, without reintroducing
    // the flicker the full-clear skip avoids. Dimensions match whichever draw
    // path runs below: unscaled full image vs scaled/rotated buffer.
    {
        bool unscaled = (scaleX == 1.0 && scaleY == 1.0 && rotationAngle == 0.0);
        int16_t drawnW = unscaled ? fullImageWidth : scaledWidth;
        int16_t drawnH = unscaled ? fullImageHeight : scaledHeight;
        eraseUncoveredPrevRegion(gfx, finalX, finalY, drawnW, drawnH);
    }

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
        // No scaling or rotation needed
        scaledBufferValid = false;  // this path doesn't populate the scaled-render cache
        int currentTemp = configStorage.getColorTemp();
        if (currentTemp != 6500) {
            // Copy to scaledBuffer first, then apply color temp to the copy.
            // This prevents compounding color shifts when the same image is re-rendered.
            size_t imageSize = fullImageWidth * fullImageHeight * sizeof(uint16_t);
            if (imageSize <= scaledBufferSize) {
                memcpy(scaledBuffer, fullImageBuffer, imageSize);
                ImageUtils::adjustColorTemperature(scaledBuffer, fullImageWidth, fullImageHeight, currentTemp);
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, fullImageWidth, fullImageHeight);
            } else {
                // Fallback: buffer too small, apply in-place (legacy behavior)
                ImageUtils::adjustColorTemperature(fullImageBuffer, fullImageWidth, fullImageHeight, currentTemp);
                displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
            }
        } else {
            displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        }

        // auto_flush (DSI ctor) already cache-syncs the drawn region

        systemMonitor.forceResetWatchdog();

        // Update tracking variables for next transition
        prevImageX = finalX;
        prevImageY = finalY;
        prevImageWidth = fullImageWidth;
        prevImageHeight = fullImageHeight;
    } else {
        // Try hardware acceleration first if available
        size_t scaledImageSize = scaledWidth * scaledHeight * 2;

        // Reuse fast-path: if only the per-image offset changed since the last
        // render (same image, scale, rotation, and color temp), the scaled+rotated
        // buffer is still valid. Re-draw it at the new position and skip the PPA
        // pass entirely. Common during interactive offset tuning.
        if (scaledBufferValid && imageGeneration == lastRenderGeneration
            && scaleX == lastRenderScaleX && scaleY == lastRenderScaleY
            && rotationAngle == lastRenderRotation
            && configStorage.getColorTemp() == lastRenderColorTemp
            && scaledImageSize <= scaledBufferSize) {
            displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);
            systemMonitor.forceResetWatchdog();
            prevImageX = finalX;
            prevImageY = finalY;
            prevImageWidth = scaledWidth;
            prevImageHeight = scaledHeight;
            return;
        }

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
            
            Serial.printf("[PPA] Attempting zero-copy hardware scale+rotate: %dx%d -> %dx%d (%.0f°)\n",
                         fullImageWidth, fullImageHeight, scaledWidth, scaledHeight, rotationAngle);

            if (ppaAccelerator.scaleRotateImageZeroCopy(fullImageBuffer, fullImageWidth, fullImageHeight,
                                              scaledBuffer, scaledBufferSize, scaledWidth, scaledHeight, rotationAngle)) {
                // Resume display after heavy operation
                displayManager.resumeDisplay();
                
                systemMonitor.forceResetWatchdog();
                
                unsigned long hwTime = millis() - hwStart;
                Serial.printf("[PPA] ✓ Hardware acceleration successful in %lu ms\n", hwTime);
                debugPrintf(COLOR_GREEN, "PPA hardware render: %lu ms", hwTime);
                
                // Apply color temperature adjustment to scaled buffer
                int currentTemp = configStorage.getColorTemp();
                if (currentTemp != 6500) {
                    ImageUtils::adjustColorTemperature(scaledBuffer, scaledWidth, scaledHeight, currentTemp);
                }
                
                // Draw the hardware-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);

                // auto_flush (DSI ctor) already cache-syncs the drawn region

                systemMonitor.forceResetWatchdog();

                // Update tracking variables for next transition
                prevImageX = finalX;
                prevImageY = finalY;
                prevImageWidth = scaledWidth;
                prevImageHeight = scaledHeight;
                // Mark the scaled buffer reusable for an offset-only re-render.
                scaledBufferValid = true;
                lastRenderScaleX = scaleX; lastRenderScaleY = scaleY;
                lastRenderRotation = rotationAngle;
                lastRenderColorTemp = configStorage.getColorTemp();
                lastRenderGeneration = imageGeneration;
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
                
                // Apply color temperature adjustment to scaled buffer
                int currentTemp = configStorage.getColorTemp();
                if (currentTemp != 6500) {
                    ImageUtils::adjustColorTemperature(scaledBuffer, scaledWidth, scaledHeight, currentTemp);
                }
                
                // Draw the software-processed image
                displayManager.drawBitmap(finalX, finalY, scaledBuffer, scaledWidth, scaledHeight);

                // auto_flush (DSI ctor) already cache-syncs the drawn region

                systemMonitor.forceResetWatchdog();

                // Update tracking variables for next transition
                prevImageX = finalX;
                prevImageY = finalY;
                prevImageWidth = scaledWidth;
                prevImageHeight = scaledHeight;
                // Mark the scaled buffer reusable for an offset-only re-render.
                scaledBufferValid = true;
                lastRenderScaleX = scaleX; lastRenderScaleY = scaleY;
                lastRenderRotation = rotationAngle;
                lastRenderColorTemp = configStorage.getColorTemp();
                lastRenderGeneration = imageGeneration;
                return;
            } else {
                Serial.println("[Render] ✗ Software scaling failed");
                debugPrint("ERROR: Software scaling failed", COLOR_RED);
            }
        }
        
        // Ultimate fallback: draw original unscaled image
        Serial.println("[Render] Drawing original unscaled image");
        debugPrint("WARNING: Showing unscaled image", COLOR_YELLOW);
        
        // Apply color temperature adjustment
        int currentTemp = configStorage.getColorTemp();
        if (currentTemp != 6500) {
            ImageUtils::adjustColorTemperature(fullImageBuffer, fullImageWidth, fullImageHeight, currentTemp);
        }
        
        displayManager.drawBitmap(finalX, finalY, fullImageBuffer, fullImageWidth, fullImageHeight);
        
        // auto_flush (DSI ctor) already cache-syncs the drawn region
        
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

// Sphere tessellation for the resting (full-resolution) moon render. 96x48 is
// NINA's resting default: smooth enough that the limb shows no faceting.
static const int MOON_REST_SECTORS = 96;
static const int MOON_REST_STACKS  = 48;

void renderMoonToPendingBuffer() {
    // Require a real wall-clock time (NTP). epoch < 2020-01-01 means unsynced.
    time_t now = time(nullptr);
    if (now < 1577836800) {
        Serial.println("[Moon] Clock not synced yet; skipping moon render this cycle");
        return;
    }

    // Decode the equirectangular lunar texture into PSRAM once (lazy: only the
    // first time a moon source is shown). ~4 MB RGB565 held for app lifetime.
    static bool moonTexReady = false;
    if (!moonTexReady) {
        moonTexReady = moon_sphere_init();
        if (!moonTexReady) {
            Serial.println("[Moon] moon_sphere_init() failed (texture decode); skipping");
            return;
        }
    }

    moon_state_t st;
    moon_compute(now, (double)configStorage.getMoonLat(),
                      (double)configStorage.getMoonLon(), &st);
    Serial.printf("[Moon] %s illum=%.2f waxing=%d lib(%.1f,%.1f)deg\n",
                  st.phase_name, st.illum, st.waxing,
                  st.lib_lon * 57.2958f, st.lib_lat * 57.2958f);

    // Render at the native panel resolution (720x720 or 800x800) so the
    // starfield/glow background fills the whole screen. The per-source scale
    // controls the moon DISK size within that frame: 1.0 = disk fills the panel
    // edge-to-edge, <1.0 shrinks the disk and the background fills the rest.
    const int w = displayManager.getWidth();
    const int h = displayManager.getHeight();
    float diskScale = (imageSourceCount > 0)
                        ? configStorage.getImageScaleX(currentImageIndex)
                        : 1.0f;
    moon_sphere_set_disk_scale(diskScale);

    uint8_t bg = (uint8_t)configStorage.getMoonBgStyle();
    uint16_t* moon = moon_sphere_render(w, h, &st,
                                        MOON_REST_SECTORS, MOON_REST_STACKS, bg);
    if (!moon) { Serial.println("[Moon] render failed"); return; }

    size_t bytes = (size_t)w * (size_t)h * 2;
    if (xSemaphoreTake(imageBufferMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        if (pendingFullImageBuffer && bytes <= fullImageBufferSize) {
            memcpy(pendingFullImageBuffer, moon, bytes);
            pendingImageWidth  = w;
            pendingImageHeight = h;
            imageReadyToDisplay = true;
            Serial.printf("[Moon] pending buffer filled %dx%d (disk %.2f), ready\n",
                          w, h, diskScale);
        } else {
            Serial.println("[Moon] pending buffer unavailable or too small");
        }
        xSemaphoreGive(imageBufferMutex);
    } else {
        Serial.println("[Moon] could not acquire image buffer mutex");
    }
    heap_caps_free(moon);
}

// Config shim: moon_interaction.c is plain C and cannot call the C++
// configStorage directly. It calls this to read the free-spin mode.
extern "C" uint8_t moon_cfg_spin_mode(void) {
    return configStorage.getMoonSpinMode();
}

// Interactive moon drag-to-rotate loop. Entered (and kept running) while
// interactiveMoonMode is set by updateTouchState(). Renders the moon small
// (240x240) with the finger-driven yaw/pitch, PPA-upscales to the panel, and
// repeats until the disc eases home (snap-back) or the free-spin hold expires
// and it returns. Blocks the main loop for the duration (watchdog fed each
// frame), then forces a crisp full-resolution resting render via lastUpdate=0.
void serviceMoonDrag() {
    if (!interactiveMoonMode) return;

    const int DS = 240;  // interactive render size, upscaled to the panel by PPA
    static uint16_t* dragColor = nullptr;
    static uint16_t* dragZ = nullptr;
    if (!dragColor) dragColor = (uint16_t*)heap_caps_aligned_alloc(128, (size_t)DS * DS * 2, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    if (!dragZ)     dragZ     = (uint16_t*)heap_caps_aligned_alloc(128, (size_t)DS * DS * 2, MALLOC_CAP_SPIRAM);
    if (!dragColor || !dragZ) { Serial.println("[Moon] drag buffer alloc failed"); interactiveMoonMode = false; return; }

    const int16_t w = displayManager.getWidth();
    const int16_t h = displayManager.getHeight();

    // Snapshot the ephemeris once: during a drag the orientation comes from the
    // finger (yaw/pitch), not from time advancing.
    time_t now = time(nullptr);
    moon_state_t st;
    moon_compute(now, (double)configStorage.getMoonLat(), (double)configStorage.getMoonLon(), &st);

    float diskScale = (imageSourceCount > 0) ? configStorage.getImageScaleX(currentImageIndex) : 1.0f;
    moon_sphere_set_disk_scale(diskScale);
    uint8_t bg = (uint8_t)configStorage.getMoonBgStyle();
    moon_light_mode_t lm = (configStorage.getMoonDragLightMode() == 1) ? MOON_LIGHT_EXPLORE : MOON_LIGHT_TRUE_PHASE;

    Serial.println("[Moon] interactive drag loop start");
    while (interactiveMoonMode) {
        updateTouchState();   // keep feeding the finger -> moon_drag_move / moon_drag_end

        // Free-spin (moon_spin_mode==1): hold the spun orientation, then ease home.
        if (!moon_drag_active() && moon_drag_freespin_pending() &&
            moon_drag_freespin_elapsed(configStorage.getMoonSpinReturnS())) {
            moon_drag_trigger_return();
        }

        moon_drag_advance(0.35f);   // ease current orientation toward target
        float yaw = 0.0f, pitch = 0.0f;
        moon_drag_get(&yaw, &pitch);

        uint16_t* frame = moon_sphere_render_into(DS, DS, &st, MOON_REST_SECTORS, MOON_REST_STACKS,
                                                  bg, yaw, pitch, lm, dragColor, dragZ);
        if (frame &&
            ppaAccelerator.scaleRotateImageZeroCopy(dragColor, DS, DS, scaledBuffer,
                                                    scaledBufferSize, w, h, 0.0f)) {
            displayManager.drawBitmap(0, 0, scaledBuffer, w, h);
        }
        systemMonitor.forceResetWatchdog();

        // Done once the finger is up, the disc has eased home, and no free-spin
        // hold is still pending.
        if (!moon_drag_active() && moon_drag_settled() && !moon_drag_freespin_pending()) {
            interactiveMoonMode = false;
        }
    }
    moon_drag_reset();
    lastUpdate = 0;   // force a prompt crisp full-resolution resting re-render
    Serial.println("[Moon] interactive drag loop end");
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

    // Reset watchdog at function start
    systemMonitor.forceResetWatchdog();

    Serial.println("DEBUG: Watchdog reset complete");

    // Enhanced network connectivity check
    if (!wifiManager.isConnected()) {
        Serial.println("ERROR: No WiFi connection");
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }

    // Network health check removed — the HTTP request itself has a timeout,
    // and the TCP probe to 8.8.8.8:53 was useless on isolated LANs.
    Serial.println("DEBUG: WiFi connection check passed");

    // Get current image URL based on cycling configuration
    String imageURL = getCurrentImageURL();
    currentImageURL = imageURL;  // keep the shared copy in sync (used by render + logs)

    // Assume failure until an image is successfully prepared; the scheduler uses
    // this to decide whether to retry sooner than the normal update interval.
    imageDownloadFailed = true;

    if (imageURL.startsWith("moon://")) {
        Serial.println("[Moon] Rendering computed moon image");
        renderMoonToPendingBuffer();
        imageDownloadFailed = !imageReadyToDisplay;  // success iff a frame is ready
        // This path returns before the shared "first image loaded" bookkeeping
        // below. When the moon is the only enabled source, that flag would never
        // be set, so on-screen debug text (e.g. the periodic HTTP time sync line)
        // would keep drawing over the moon and flicker on each refresh. Mark it
        // here once a moon frame is ready so the debug overlay is suppressed.
        if (imageReadyToDisplay && !firstImageLoaded) {
            firstImageLoaded = true;
            displayManager.setFirstImageLoaded(true);
            Serial.println("First image (moon) ready - suppressing on-screen debug");
        }
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;   // matches the early-return contract used below
        return;
    }

    Serial.println("DEBUG: About to get current image URL");
    Serial.printf("DEBUG: Current image URL: %s\n", imageURL.c_str());
    
    // Reset watchdog before HTTP operations
    systemMonitor.forceResetWatchdog();
    
    Serial.println("[Image] ===== Starting Image Download =====");
    Serial.printf("[Image] URL: %s\n", imageURL.c_str());
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
    // getSize() returns -1 when the server sends no Content-Length (e.g. chunked
    // transfer encoding). Read it as a signed int first: a previous size_t cast
    // turned -1 into SIZE_MAX, which tripped the "too large" check below and
    // silently rejected every chunked / no-Content-Length source.
    int contentLength = http.getSize();
    bool knownLength = (contentLength > 0);
    // For unknown-length streams, read until the connection closes, bounded by
    // the download buffer (the read loop guards bytesRead + chunk > imageBufferSize).
    size_t size = knownLength ? (size_t)contentLength : imageBufferSize;

    Serial.printf("[Image] Content-Length: %d bytes%s\n", contentLength, knownLength ? "" : " (unknown/chunked)");
    if (cyclingEnabled && imageSourceCount > 1) {
        Serial.printf("[Image] Source: Image %d/%d - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL.c_str());
        debugPrintf(COLOR_WHITE, "Image %d/%d: %d bytes", currentImageIndex + 1, imageSourceCount, contentLength);
    } else {
        Serial.printf("[Image] Source: %s\n", currentImageURL.c_str());
        debugPrintf(COLOR_WHITE, "Image: %d bytes", contentLength);
    }

    // A declared length of exactly 0 is an empty/invalid response.
    if (contentLength == 0) {
        LOG_WARNING("[ImageDownload] Content-Length is zero - aborting download");
        Serial.println("[Image] ⚠ Content-Length is zero - aborting");
        debugPrintf(COLOR_RED, "Invalid size: 0 bytes");
        http.end();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;  // Clear mutex before return
        return;
    }

    // Reject only when the server declares a size that will not fit. Unknown-length
    // streams are capped by the read loop's buffer-overflow guard instead.
    if (knownLength && (size_t)contentLength >= imageBufferSize) {
        LOG_ERROR_F("[ImageDownload] Image too large: %d bytes exceeds buffer capacity of %d bytes\n", contentLength, imageBufferSize);
        Serial.printf("[Image] ✗ Image too large! %d bytes exceeds buffer %d bytes\n", contentLength, imageBufferSize);
        debugPrintf(COLOR_RED, "Invalid size: %d bytes", contentLength);
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
    const size_t ULTRA_CHUNK_SIZE = 8192;  // 8KB chunks for better throughput

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
    
    // Validate download completeness (only when the server declared a length;
    // for chunked/unknown-length streams, a clean connection close IS the end).
    if (knownLength && bytesRead < size) {
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
        // Choose a decode-time downscale (1/1, 1/2, 1/4, 1/8) so oversized
        // full-disc sources (e.g. GOES-19 1808px, which would need ~6.5MB at
        // full size) fit the RGB565 image buffer. Normal ~1024px sources keep
        // full resolution (divisor 1).
        int srcW = jpeg.getWidth();
        int srcH = jpeg.getHeight();
        int decodeDiv = 1;
        int decodeOptions = 0;
        while (decodeDiv < 8) {
            int dw = srcW / decodeDiv, dh = srcH / decodeDiv;
            if ((size_t)dw * dh * 2 <= fullImageBufferSize && dw <= MAX_IMAGE_DIMENSION && dh <= MAX_IMAGE_DIMENSION) break;
            decodeDiv *= 2;
        }
        if (decodeDiv == 2)      decodeOptions = JPEG_SCALE_HALF;
        else if (decodeDiv == 4) decodeOptions = JPEG_SCALE_QUARTER;
        else if (decodeDiv == 8) decodeOptions = JPEG_SCALE_EIGHTH;
        if (decodeDiv > 1) {
            Serial.printf("[Image] Downscaling %dx%d by 1/%d to fit buffer\n", srcW, srcH, decodeDiv);
        }
        pendingImageWidth = srcW / decodeDiv;
        pendingImageHeight = srcH / decodeDiv;

        // Check if image fits in our pending buffer
        size_t requiredSize = pendingImageWidth * pendingImageHeight * 2;
        Serial.printf("[Image] Buffer check: %d bytes required, %d bytes available\n", requiredSize, fullImageBufferSize);
        Serial.printf("DEBUG: Image size check - Required: %d bytes, Available: %d bytes\n", requiredSize, fullImageBufferSize);
        
        if (requiredSize <= fullImageBufferSize) {
            Serial.println("DEBUG: Image fits in buffer, proceeding with decode...");

            // Take mutex to protect pendingFullImageBuffer during decode
            if (xSemaphoreTake(imageBufferMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
                Serial.println("ERROR: Failed to acquire image buffer mutex for decode");
                jpeg.close();
                imageProcessing = false;
                return;
            }

            // Clear only the bottom MCU band of the PENDING buffer. The JPEG
            // decoder overwrites every full-MCU row, so a full-buffer clear is
            // redundant; only the final partial-MCU rows (skipped by the bounds
            // guard in JPEGDraw when the height isn't MCU-aligned) need zeroing
            // to avoid leftover pixels from a previous, taller image.
            {
                int bandRows = 16;
                if (bandRows > pendingImageHeight) bandRows = pendingImageHeight;
                int bandStart = pendingImageHeight - bandRows;
                memset(pendingFullImageBuffer + (size_t)bandStart * pendingImageWidth, 0,
                       (size_t)bandRows * pendingImageWidth * sizeof(uint16_t));
            }

            // Decode the full image into PENDING buffer with watchdog protection
            systemMonitor.forceResetWatchdog();

            unsigned long decodeStart = millis();

            // JPEG decode with timeout monitoring

            unsigned long decodeStartTime = millis();

            Serial.println("[Image] Decoding JPEG to RGB565...");
            if (jpeg.decode(0, 0, decodeOptions)) {
                unsigned long decodeTime = millis() - decodeStart;
                Serial.printf("[Image] ✓ Decode complete in %lu ms\n", decodeTime);
                Serial.printf("[Image] Decoded %dx%d pixels (%d bytes RGB565)\n",
                             pendingImageWidth, pendingImageHeight,
                             pendingImageWidth * pendingImageHeight * 2);

                // Reset watchdog after decode
                systemMonitor.forceResetWatchdog();

                // Mark image as ready to display (but don't display yet - let loop handle it)
                imageReadyToDisplay = true;
                imageDownloadFailed = false;  // success: a frame is ready for the swap

                // Release mutex after marking image ready
                xSemaphoreGive(imageBufferMutex);

                if (cyclingEnabled && imageSourceCount > 1) {
                    Serial.printf("[Image] Image %d/%d ready to display - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL.c_str());
                    debugPrintf(COLOR_GREEN, "Image %d/%d ready", currentImageIndex + 1, imageSourceCount);
                } else {
                    Serial.printf("[Image] Image ready to display - %s\n", currentImageURL.c_str());
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
                // Release mutex on decode failure
                xSemaphoreGive(imageBufferMutex);

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
    
    // Migration: Convert legacy single-image mode to unified list mode
    if (!cyclingEnabled && imageSourceCount == 0) {
        String legacyURL = configStorage.getImageURL();
        if (legacyURL.length() > 0) {
            Serial.println("Migrating legacy single-image configuration to unified list mode...");
            configStorage.addImageSource(legacyURL);
            configStorage.setCyclingEnabled(true);
            configStorage.saveConfig();
            
            // Reload config after migration
            cyclingEnabled = true;
            imageSourceCount = 1;
            Serial.printf("Migration complete: URL '%s' added to image source list\n", legacyURL.c_str());
        }
    }
    
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

        // The computed moon is rendered at native panel resolution with its disk
        // size already applied inside the renderer (driven by scaleX). Draw that
        // full-screen buffer 1:1 so the PPA pipeline does not re-scale it (which
        // would double-apply the scale and crop the background). Pan offsets are
        // kept so the user can still nudge the moon.
        currentSourceIsMoon = configStorage.getImageSource(index).startsWith("moon://");
        if (currentSourceIsMoon) {
            scaleX = 1.0f;
            scaleY = 1.0f;
            rotationAngle = 0.0f;
        }

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
    
    int attempts = 0;
    int maxAttempts = imageSourceCount * 2;  // Prevent infinite loop
    
    if (randomOrderEnabled) {
        // Random order: pick a different random enabled image
        int newIndex;
        do {
            newIndex = random(0, imageSourceCount);
            attempts++;
            if (attempts >= maxAttempts) {
                Serial.println("WARNING: Could not find enabled image after maximum attempts");
                break;
            }
        } while ((newIndex == currentImageIndex || !configStorage.isImageEnabled(newIndex)) && imageSourceCount > 1);
        
        currentImageIndex = newIndex;
    } else {
        // Sequential order: advance to next enabled image
        int startIndex = currentImageIndex;
        do {
            currentImageIndex = (currentImageIndex + 1) % imageSourceCount;
            attempts++;
            if (attempts >= maxAttempts) {
                Serial.println("WARNING: Could not find enabled image after maximum attempts");
                currentImageIndex = startIndex;  // Revert to original
                break;
            }
        } while (!configStorage.isImageEnabled(currentImageIndex));
    }
    
    // Save the new index to persistent storage
    configStorage.setCurrentImageIndex(currentImageIndex);
    configStorage.saveConfig();
    
    // Update transform settings for the new image
    updateCurrentImageTransformSettings();
    
    Serial.printf("Advanced to image %d/%d (enabled=%s): %s\n", 
                  currentImageIndex + 1, imageSourceCount, 
                  configStorage.isImageEnabled(currentImageIndex) ? "yes" : "no",
                  getCurrentImageURL().c_str());
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
    // Skip during OTA to free network bandwidth and reduce PSRAM contention
    if (!webConfig.isOTAInProgress()) {
        taskRetryHandler.process();
    }
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

    // Cache NVS configuration reads — refresh every 5 seconds to reduce flash wear
    static unsigned long lastConfigRefresh = 0;
    static unsigned long cachedUpdateInterval = 0;
    static int cachedImageUpdateMode = 0;
    static unsigned long cachedImageDuration = 0;
    static int cachedImageDurationIndex = -1;  // Track which index was cached
    if (loopStartTime - lastConfigRefresh > 5000 || lastConfigRefresh == 0) {
        cachedUpdateInterval = configStorage.getUpdateInterval();
        cachedImageUpdateMode = configStorage.getImageUpdateMode();
        cachedImageDuration = configStorage.getImageDuration(currentImageIndex);
        cachedImageDurationIndex = currentImageIndex;
        lastConfigRefresh = loopStartTime;
    }
    // If image index changed since last cache, refresh duration immediately
    if (cachedImageDurationIndex != currentImageIndex) {
        cachedImageDuration = configStorage.getImageDuration(currentImageIndex);
        cachedImageDurationIndex = currentImageIndex;
    }

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
        } else if (!webConfig.isOTAInProgress()) {
            // Try to start web server if not running (skip during OTA)
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
    // Skip during OTA to free network bandwidth
    if (wifiManager.isConnected() && !webConfig.isOTAInProgress()) {
        unsigned long mqttStartTime = millis();
        mqttManager.update();

        // Check if MQTT update took too long
        if (millis() - mqttStartTime > 2000) {
            Serial.printf("WARNING: MQTT update took %lu ms\n", millis() - mqttStartTime);
        }
        systemMonitor.forceResetWatchdog();
    }

    // Handle WebSocket events — skip during OTA to free network bandwidth
    if (wifiManager.isConnected() && webConfig.isRunning() && !webConfig.isOTAInProgress()) {
        webConfig.loopWebSocket();
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
    
    // Update touch state
    if (touchEnabled) {
        updateTouchState();
        systemMonitor.forceResetWatchdog();
        // If a finger started rotating the moon, run the interactive drag loop
        // here (blocks until the disc settles), then resume the normal loop.
        if (interactiveMoonMode) {
            serviceMoonDrag();
            systemMonitor.forceResetWatchdog();
        }
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
    
    // Update dynamic configuration from cached NVS values
    currentUpdateInterval = cachedUpdateInterval;
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
    
    // Tune mode uses an explicit-exit model: the web UI clears the editing
    // pause via the Done/resume button (/api/images/tune/stop). This timer is
    // only a long safety backstop in case the client never sends that request
    // (e.g. tab closed mid-tune), not the normal exit path.
    static const unsigned long EDIT_HOLD_BACKSTOP_MS = 600000UL; // 10 min safety net; normal exit is the Done/resume button
    if (cyclingPausedForEditing && (currentTime - lastEditActivity > EDIT_HOLD_BACKSTOP_MS)) {
        cyclingPausedForEditing = false;
        Serial.println("DEBUG: Resuming automatic cycling after edit-hold backstop");
    }
    
    // Only auto-cycle if in automatic mode, not paused, and multiple images available
    // Skip during OTA — no point cycling images while firmware is being written
    int imageUpdateMode = cachedImageUpdateMode;
    if (imageUpdateMode == 0 && cyclingEnabled && imageSourceCount > 1 && !imageProcessing && !singleImageRefreshMode && !cyclingPausedForEditing && !webConfig.isOTAInProgress()) {
        // Use per-image duration instead of global cycle interval (from cached NVS value)
        unsigned long currentImageDuration = cachedImageDuration * 1000;  // Convert seconds to milliseconds
        if (currentTime - lastCycleTime >= currentImageDuration || lastCycleTime == 0) {
            shouldCycle = true;
            lastCycleTime = currentTime;
            Serial.println("DEBUG: Time to cycle to next image source (Automatic Mode)");
            advanceToNextImage();
        }
    }
    
    // Check if it's time to update the image (either scheduled update or cycling)
    // In API mode, only update on force refresh (lastUpdate = 0) or scheduled refresh interval
    // Skip image processing during OTA to prevent interference
    bool normalDue = false;
    if (imageUpdateMode == 0) {
        // Automatic cycling mode: normal behavior
        normalDue = (shouldCycle || currentTime - lastUpdate >= currentUpdateInterval || lastUpdate == 0);
    } else {
        // API-triggered mode: only update when explicitly triggered (lastUpdate = 0)
        normalDue = (lastUpdate == 0);
    }
    // Fast retry after a failed download: re-attempt a few times before falling
    // back to the normal interval, so a transient network blip recovers in
    // seconds instead of leaving a stale/blank screen for a full update cycle.
    static const int MAX_DOWNLOAD_RETRIES = 3;
    static const unsigned long DOWNLOAD_RETRY_DELAY_MS = 15000;
    bool retryDue = imageDownloadFailed && downloadRetryCount < MAX_DOWNLOAD_RETRIES
                    && lastUpdate != 0 && (currentTime - lastUpdate >= DOWNLOAD_RETRY_DELAY_MS);
    bool shouldUpdate = normalDue || retryDue;
    
    if (!imageProcessing && !imageDownloadPending && !webConfig.isOTAInProgress() && shouldUpdate) {
        // Pre-download system health check
        if (!wifiManager.isConnected()) {
            // Rate-limit this log so it doesn't spam every 50ms loop
            static unsigned long lastWifiSkipLog = 0;
            if (currentTime - lastWifiSkipLog > 5000) {
                Serial.println("WARNING: WiFi disconnected, skipping image download");
                lastWifiSkipLog = currentTime;
            }
            // Do NOT update lastUpdate here — leave it at 0 (or stale) so the
            // download triggers immediately once WiFi connects.  The old code
            // set lastUpdate = currentTime which forced a full updateInterval
            // (2 min) wait after WiFi came up, leaving the display stuck on
            // the "Connecting to WiFi..." screen.
            systemMonitor.forceResetWatchdog();
        } else {
            Serial.printf("DEBUG: Triggering async image download (last update: %lu ms ago)\n", 
                         currentTime - lastUpdate);
            
            // Trigger async download on Core 0
            imageDownloadPending = true;
            lastUpdate = currentTime;
            lastImageProcessTime = currentTime;
            // Count consecutive fast-retries; a normal/cycle trigger resets it.
            downloadRetryCount = (retryDue && !normalDue) ? (downloadRetryCount + 1) : 0;

            systemMonitor.forceResetWatchdog();
        }
    }
    
    // Check if new image is ready to display - swap buffers for seamless transition (NO FLICKER!)
    // Skip image rendering during OTA to prevent display interference
    if (imageReadyToDisplay && !webConfig.isOTAInProgress()) {
        // Take mutex to protect buffer swap from concurrent decode writes
        if (xSemaphoreTake(imageBufferMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            imageReadyToDisplay = false;  // Clear the flag under mutex protection

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

            // New image is now active; invalidate the scaled-render reuse cache
            // so the next render recomputes instead of redrawing the old scale.
            imageGeneration++;

            xSemaphoreGive(imageBufferMutex);

            Serial.printf("Buffer swap complete: %dx%d image now active\n", fullImageWidth, fullImageHeight);
            systemMonitor.forceResetWatchdog();

            // Refresh the live transform globals (scale/offset/rotation) from the
            // current image's stored config before drawing. Without this, edits made
            // outside tune mode persist to NVS but never reach the live render: this
            // swap path does not otherwise reload them, so the image keeps its stale
            // offset until the next source cycle or reboot. updateCurrentImageTransformSettings
            // reads currentImageIndex, which is the image just prepared in this swap.
            updateCurrentImageTransformSettings();

            // Now render the new image to display (single seamless update, no clearing artifacts)
            if (cyclingEnabled && imageSourceCount > 1) {
                Serial.printf("[Image] Rendering image %d/%d - %s\n", currentImageIndex + 1, imageSourceCount, currentImageURL.c_str());
                debugPrintf(COLOR_GREEN, "Rendering image %d/%d", currentImageIndex + 1, imageSourceCount);
            } else {
                Serial.printf("[Image] Rendering image - %s\n", currentImageURL.c_str());
                debugPrintf(COLOR_GREEN, "Rendering image");
            }
            renderFullImage();
            systemMonitor.forceResetWatchdog();

            Serial.println("Image display completed - no flicker!");
        } else {
            Serial.println("WARNING: Could not acquire mutex for buffer swap, will retry next loop");
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
        if (currentTime - touchReleaseTime < TOUCH_DEBOUNCE_MS ||
            currentTime - touchPressTime < TOUCH_DEBOUNCE_MS) {
            return; // Too soon since last state change
        }
    }
    
    // Update touch state
    touchWasPressed = touchPressed;
    touchPressed = currentlyPressed;

    // --- Moon drag-to-rotate detection -------------------------------------
    // Runs alongside the tap/double-tap machine. On a moon frame, finger travel
    // past the threshold promotes the gesture to a rotate and hands off to the
    // interactive render loop (serviceMoonDrag), suppressing tap-to-advance.
    int curX = (touchData.cnt > 0) ? (int)touchData.x[0] : 0;
    int curY = (touchData.cnt > 0) ? (int)touchData.y[0] : 0;
    if (touchPressed && !touchWasPressed) {
        moonTouchStartX = curX;
        moonTouchStartY = curY;
        moonDragCandidate = currentSourceIsMoon;
    }
    if (touchPressed && moonDragCandidate) {
        int dxm = curX - moonTouchStartX;
        int dym = curY - moonTouchStartY;
        if (!interactiveMoonMode &&
            (dxm * dxm + dym * dym) >= MOON_DRAG_THRESHOLD_PX * MOON_DRAG_THRESHOLD_PX) {
            // Promote to a rotate: begin from the press point, hand to the loop.
            interactiveMoonMode = true;
            moon_drag_begin((float)moonTouchStartX, (float)moonTouchStartY);
            moon_drag_move((float)curX, (float)curY);
            Serial.println("[Moon] drag-to-rotate engaged");
        } else if (interactiveMoonMode) {
            moon_drag_move((float)curX, (float)curY);
        }
    }
    if (interactiveMoonMode) {
        // The interactive loop owns the gesture; skip tap/double-tap handling so
        // a rotate never also advances the slideshow or toggles mode.
        if (!touchPressed && touchWasPressed) {
            moon_drag_end();
        }
        return;
    }

    // On the moon page ONLY sphere rotation (the drag handled above) is active.
    // Suppress every page-navigation gesture (single tap = next image, double
    // tap = mode toggle) so a tap or stray touch while viewing the moon never
    // navigates away or changes mode. Keep the tap state machine idle so no
    // stale tap carries over when the slideshow later leaves the moon source.
    if (currentSourceIsMoon) {
        touchState = TOUCH_IDLE;
        firstTapTime = 0;
        touchTriggeredNextImage = false;
        touchTriggeredModeToggle = false;
        return;
    }

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
                        Serial.printf("Touch: Double-tap detected! (total time: %lu ms)\n",
                                     currentTime - firstTapTime);
                        firstTapTime = 0;
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


