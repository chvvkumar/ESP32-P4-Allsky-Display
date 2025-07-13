#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// SYSTEM CONFIGURATION
// =============================================================================

// Memory allocation sizes
#define IMAGE_BUFFER_MULTIPLIER 1        // Display size multiplier for image buffer
#define FULL_IMAGE_BUFFER_SIZE (1024 * 1024)  // 1MB for full image buffer (512x512 max image at 16-bit)
#define SCALED_BUFFER_MULTIPLIER 4       // 4x display size to handle large scale factors

// System timing intervals (milliseconds)
#define UPDATE_INTERVAL 300000           // 5 minutes between image updates
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

// MQTT topics
extern const char* MQTT_REBOOT_TOPIC;
extern const char* MQTT_BRIGHTNESS_TOPIC;
extern const char* MQTT_BRIGHTNESS_STATUS_TOPIC;

// =============================================================================
// IMAGE CONFIGURATION
// =============================================================================

// Image source URL (legacy single image support)
extern const char* IMAGE_URL;

// Multi-image cycling configuration
#define MAX_IMAGE_SOURCES 10             // Maximum number of image sources
#define DEFAULT_CYCLE_INTERVAL 60000     // 1 minute between image switches
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
#define MAX_SCALE 3.0f                   // Maximum scale factor
#define ROTATION_STEP 90.0f              // Rotation increment in degrees

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
#define DEBUG_START_Y 50                 // Starting Y position for debug text
#define DEBUG_LINE_HEIGHT 30             // Height between debug lines
#define DEBUG_TEXT_SIZE 3                // Debug text size
#define MAX_DEBUG_LINES 20               // Maximum lines to show on screen

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

#define WATCHDOG_TIMEOUT_MS 15000        // 15 second timeout for faster recovery
#define WATCHDOG_IDLE_CORE_MASK 0        // Don't monitor idle tasks
#define WATCHDOG_TRIGGER_PANIC false     // Don't panic on timeout, just reset

// =============================================================================
// NETWORK TIMEOUT CONFIGURATION
// =============================================================================

#define HTTP_CONNECT_TIMEOUT 8000        // 8 second connection timeout
#define HTTP_REQUEST_TIMEOUT 10000       // 10 second request timeout
#define DNS_RESOLUTION_TIMEOUT 5000      // 5 second DNS timeout
#define NETWORK_CHECK_TIMEOUT 3000       // 3 second network connectivity check
#define HTTP_BEGIN_TIMEOUT 5000          // 5 second timeout for http.begin()
#define DOWNLOAD_CHUNK_TIMEOUT 2000      // 2 second timeout per download chunk
#define TOTAL_DOWNLOAD_TIMEOUT 15000     // 15 second total download timeout

// =============================================================================
// CONFIGURATION FUNCTIONS
// =============================================================================

// Initialize configuration from persistent storage
void initializeConfiguration();

// Reload configuration from storage (useful after web config changes)
void reloadConfiguration();

#endif // CONFIG_H
