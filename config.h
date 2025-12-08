#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// LOG SEVERITY LEVELS
// =============================================================================

enum LogSeverity {
    LOG_DEBUG = 0,      // Verbose debugging information
    LOG_INFO = 1,       // General informational messages
    LOG_WARNING = 2,    // Warning messages (non-critical issues)
    LOG_ERROR = 3,      // Error messages (failures, but system continues)
    LOG_CRITICAL = 4    // Critical errors (system instability)
};

// Default log level for WebSocket console filtering
#define DEFAULT_LOG_LEVEL LOG_DEBUG  // Show all messages by default

// =============================================================================
// SYSTEM CONFIGURATION
// =============================================================================

// Memory allocation sizes
// NOTE: These values automatically control PPA hardware accelerator buffer sizes:
//   - PPA source buffer = FULL_IMAGE_BUFFER_SIZE
//   - PPA destination buffer = display_size * SCALED_BUFFER_MULTIPLIER * 2
//   Changing these values will automatically adjust all dependent allocations
#define IMAGE_BUFFER_MULTIPLIER 1        // Display size multiplier for image buffer
#define FULL_IMAGE_BUFFER_SIZE (4 * 1024 * 1024)  // 4MB for full image buffer (1448x1448 max image at RGB565)
#define SCALED_BUFFER_MULTIPLIER 4       // 4x display size to handle large scale factors

// System timing intervals (milliseconds)
#define UPDATE_INTERVAL 120000           // 2 minutes between image updates
#define FORCE_CHECK_INTERVAL 900000      // Force check every 15 minutes regardless of cache headers
#define WATCHDOG_RESET_INTERVAL 1000     // Reset watchdog every 1 second
#define MEMORY_CHECK_INTERVAL 30000      // Check memory every 30 seconds
#define SERIAL_FLUSH_INTERVAL 5000       // Flush serial every 5 seconds
#define IMAGE_PROCESS_TIMEOUT 5000       // 5 second timeout for image processing
#define MQTT_RECONNECT_INTERVAL 5000     // 5 seconds between MQTT reconnect attempts

// Memory thresholds
#define CRITICAL_HEAP_THRESHOLD 50000    // Less than 50KB heap is critical
#define CRITICAL_PSRAM_THRESHOLD 100000  // Less than 100KB PSRAM is critical

// =============================================================================
// WIFI CONFIGURATION
// =============================================================================

// WiFi credentials - Update these with your credentials
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// WiFi connection parameters
#define WIFI_MAX_ATTEMPTS 15             // Maximum connection attempts
#define WIFI_MAX_WAIT_TIME 12000         // 12 seconds maximum wait time
#define WIFI_RETRY_DELAY 400             // Delay between connection attempts

// =============================================================================
// MQTT CONFIGURATION
// =============================================================================

// MQTT broker settings - Update these with your MQTT broker details
extern const char* MQTT_SERVER;
extern int MQTT_PORT;
extern const char* MQTT_USER;           // Leave empty if no authentication
extern const char* MQTT_PASSWORD;       // Leave empty if no authentication
extern const char* MQTT_CLIENT_ID;

// =============================================================================
// IMAGE CONFIGURATION
// =============================================================================

// Image source URL (legacy single image support)
extern const char* IMAGE_URL;

// Multi-image cycling configuration
#define MAX_IMAGE_SOURCES 10             // Maximum number of image sources
#define DEFAULT_CYCLE_INTERVAL 30000     // 30 seconds between image switches
#define MIN_CYCLE_INTERVAL 10000         // Minimum 10 seconds between switches
#define MAX_CYCLE_INTERVAL 3600000       // Maximum 1 hour between switches

// Default image sources array (configure your image URLs here)
extern const char* DEFAULT_IMAGE_SOURCES[];
extern const int DEFAULT_IMAGE_SOURCE_COUNT;
extern const bool DEFAULT_CYCLING_ENABLED;
extern const bool DEFAULT_RANDOM_ORDER;

// Image transformation defaults
#define DEFAULT_SCALE_X 1.2f
#define DEFAULT_SCALE_Y 1.2f
#define DEFAULT_OFFSET_X 0
#define DEFAULT_OFFSET_Y 0
#define DEFAULT_ROTATION 0.0f

// Image control constants
#define SCALE_STEP 0.1f                  // Scale increment/decrement
#define MOVE_STEP 10                     // Movement step in pixels
#define MIN_SCALE 0.1f                   // Minimum scale factor
// MAX_SCALE calculated dynamically from SCALED_BUFFER_MULTIPLIER to prevent buffer overflow
// Formula: sqrt(SCALED_BUFFER_MULTIPLIER) gives max scale before exceeding buffer
// Example: SCALED_BUFFER_MULTIPLIER=4 allows 2.0x scale (4x pixels), =9 allows 3.0x, etc.
#define MAX_SCALE (sqrtf((float)SCALED_BUFFER_MULTIPLIER))  // Maximum scale factor (dynamic)
#define ROTATION_STEP 90.0f              // Rotation increment in degrees

// Helper macros for buffer size calculations
#define MAX_IMAGE_DIMENSION (int)(sqrtf(FULL_IMAGE_BUFFER_SIZE / 2))  // Max width or height (RGB565 = 2 bytes/pixel)

// =============================================================================
// DISPLAY CONFIGURATION
// =============================================================================

// Brightness control
#define BACKLIGHT_PIN 26                 // GPIO26 from schematic (LCD_BL_PWM)
#define BACKLIGHT_CHANNEL 0              // LEDC channel
#define BACKLIGHT_FREQ 5000              // 5kHz PWM frequency  
#define BACKLIGHT_RESOLUTION 10          // 10-bit resolution (0-1023)
#define DEFAULT_BRIGHTNESS 50            // Default brightness percentage

// Debug display settings
#define DEBUG_START_Y 150                // Starting Y position for debug text (centered vertically)
#define DEBUG_LINE_HEIGHT 35             // Height between debug lines
#define DEBUG_TEXT_SIZE 3                // Debug text size
#define MAX_DEBUG_LINES 15               // Maximum lines to show on screen

// Display colors (RGB565)
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_CYAN 0x07FF
#define COLOR_MAGENTA 0xF81F

// =============================================================================
// OTA CONFIGURATION
// =============================================================================

#define OTA_SERVER_PORT 80               // Web server port for OTA
#define OTA_PROGRESS_INTERVAL 1000       // Log OTA progress every 1 second

// =============================================================================
// WATCHDOG CONFIGURATION
// =============================================================================

#define WATCHDOG_TIMEOUT_MS 30000        // 30 second timeout to handle slow downloads
#define WATCHDOG_IDLE_CORE_MASK 0        // Don't monitor idle tasks
#define WATCHDOG_TRIGGER_PANIC false     // Don't panic on timeout, just reset

// =============================================================================
// TOUCH GESTURE TIMING CONFIGURATION
// =============================================================================

#define TOUCH_DEBOUNCE_MS 50             // Minimum time between touch events
#define DOUBLE_TAP_TIMEOUT_MS 400        // Maximum time between taps for double-tap
#define MIN_TAP_DURATION_MS 50           // Minimum press duration for valid tap
#define MAX_TAP_DURATION_MS 1000         // Maximum press duration for valid tap

// =============================================================================
// DOWNLOAD CONFIGURATION
// =============================================================================

#define DOWNLOAD_CHUNK_SIZE 1024         // 1KB chunks for good performance
#define DOWNLOAD_WATCHDOG_INTERVAL 50    // Reset watchdog every 50ms during download
#define DOWNLOAD_NO_DATA_TIMEOUT 5000    // 5 seconds with no data before giving up
#define DECODE_TIMEOUT 5000              // 5 second timeout for JPEG decode
#define ABSOLUTE_DOWNLOAD_TIMEOUT 50000  // 50 second absolute timeout for entire download

// =============================================================================
// SYSTEM STARTUP DELAYS
// =============================================================================

#define SERIAL_INIT_DELAY 1000           // Delay after Serial.begin() for initialization
#define WIFI_HARDWARE_INIT_DELAY 500     // Critical delay for WiFi hardware initialization
#define CAPTIVE_PORTAL_TIMEOUT 300000    // 5 minutes timeout for captive portal
#define WIFI_CONFIG_SUCCESS_DELAY 2000   // Delay after WiFi config before reboot
#define CRASH_LOG_SAVE_DELAY 100         // Delay for crash log save before reboot
#define GENERAL_ERROR_HALT_DELAY 1000    // Delay in error halt loops
#define FINAL_STARTUP_DELAY 1000         // Final delay at end of setup()
#define MESSAGE_SEND_DELAY 1000          // Delay to allow message transmission

// =============================================================================
// LOOP TIMING THRESHOLDS
// =============================================================================

#define LOOP_WARNING_THRESHOLD 1000      // Log warning if loop takes more than 1 second
#define MAIN_LOOP_DELAY 50               // Delay between main loop iterations
#define CAPTIVE_PORTAL_LOOP_DELAY 10     // Delay in captive portal handling loop
#define DOWNLOAD_CHECK_INTERVAL 1000     // Log download progress every 1 second
#define DOWNLOAD_WARNING_THRESHOLD 15000 // Warn if download takes more than 15 seconds

// =============================================================================
// NETWORK TIMEOUT CONFIGURATION
// =============================================================================

#define HTTP_CONNECT_TIMEOUT 8000        // 8 second connection timeout
#define HTTP_REQUEST_TIMEOUT 10000       // 10 second request timeout
#define DNS_RESOLUTION_TIMEOUT 5000      // 5 second DNS timeout
#define NETWORK_CHECK_TIMEOUT 3000       // 3 second network connectivity check
#define HTTP_BEGIN_TIMEOUT 5000          // 5 second timeout for http.begin()
#define DOWNLOAD_CHUNK_TIMEOUT 8000      // 8 second timeout per download chunk (for larger 4MB images)
#define TOTAL_DOWNLOAD_TIMEOUT 90000     // 90 second total download timeout (for larger 4MB images)

// =============================================================================
// CONFIGURATION FUNCTIONS
// =============================================================================

// Initialize configuration from persistent storage
void initializeConfiguration();

// Reload configuration from storage (useful after web config changes)
void reloadConfiguration();

#endif // CONFIG_H
