#include "display_manager.h"
#include "system_monitor.h"
#include "config_storage.h"

// Global instance
DisplayManager displayManager;

DisplayManager::DisplayManager() :
    dsipanel(nullptr),
    gfx(nullptr),
    displayWidth(0),
    displayHeight(0),
    displayBrightness(DEFAULT_BRIGHTNESS),
    brightnessInitialized(false),
    debugY(DEBUG_START_Y),
    debugLineCount(0),
    firstImageLoaded(false)
{
}

DisplayManager::~DisplayManager() {
    cleanup();
}

bool DisplayManager::begin() {
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
        return false;
    }
    Serial.println("DSI panel created successfully");
    
    gfx = new Arduino_DSI_Display(
        display_cfg.width,
        display_cfg.height,
        dsipanel,
        display_cfg.rotation,
        true,
        display_cfg.lcd_rst,
        display_cfg.init_cmds,
        display_cfg.init_cmds_size);
    
    if (!gfx) {
        Serial.println("ERROR: Failed to create display object!");
        return false;
    }
    Serial.println("Display object created successfully");
    
#ifdef GFX_EXTRA_PRE_INIT
    Serial.println("Running GFX_EXTRA_PRE_INIT");
    GFX_EXTRA_PRE_INIT();
#endif

    Serial.println("Starting display initialization...");
    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        return false;
    }
    Serial.println("Display initialized successfully!");
    
    displayWidth = gfx->width();
    displayHeight = gfx->height();
    Serial.printf("Display size: %dx%d\n", displayWidth, displayHeight);
    
    // Clear screen and start debug output
    clearScreen();
    debugY = DEBUG_START_Y;
    debugLineCount = 0;
    
    return true;
}

void DisplayManager::cleanup() {
    if (gfx) {
        delete gfx;
        gfx = nullptr;
    }
    if (dsipanel) {
        delete dsipanel;
        dsipanel = nullptr;
    }
}

Arduino_DSI_Display* DisplayManager::getGFX() const {
    return gfx;
}

int16_t DisplayManager::getWidth() const {
    return displayWidth;
}

int16_t DisplayManager::getHeight() const {
    return displayHeight;
}

bool DisplayManager::initBrightness() {
    if (brightnessInitialized) return true;
    
    debugPrint("Initializing brightness control...", COLOR_YELLOW);
    
    // Configure LEDC for backlight control using newer API
    if (ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION) == 0) {
        debugPrint("ERROR: LEDC attach failed!", COLOR_RED);
        return false;
    }
    
    // Get the default brightness from configuration storage
    displayBrightness = configStorage.getDefaultBrightness();
    debugPrintf(COLOR_WHITE, "Using stored brightness: %d%%", displayBrightness);
    
    // Set initial brightness
    setBrightness(displayBrightness);
    brightnessInitialized = true;
    
    debugPrint("Brightness control initialized!", COLOR_GREEN);
    return true;
}

void DisplayManager::setBrightness(int brightness) {
    if (!brightnessInitialized) return;
    
    brightness = constrain(brightness, 0, 100);
    displayBrightness = brightness;
    
    // Convert percentage to inverted 10-bit duty cycle (0-1023)
    // Backlight uses inverted logic: low PWM = high brightness
    uint32_t duty = 1023 - ((1023 * brightness) / 100);
    
    ledcWrite(BACKLIGHT_PIN, duty);
    
    Serial.printf("Display brightness set to: %d%% (PWM duty: %d)\n", brightness, duty);
}

int DisplayManager::getBrightness() const {
    return displayBrightness;
}

void DisplayManager::debugPrint(const char* message, uint16_t color) {
    if (firstImageLoaded) return; // Don't show debug after first image loads
    if (!gfx) return;
    
    Serial.println(message); // Also print to serial
    
    gfx->setTextSize(DEBUG_TEXT_SIZE);
    gfx->setTextColor(color);
    
    // Calculate text width for centering
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    gfx->getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Center the text horizontally
    int16_t centerX = (displayWidth - textWidth) / 2;
    if (centerX < 10) centerX = 10; // Minimum margin
    
    gfx->setCursor(centerX, debugY);
    gfx->println(message);
    
    debugY += DEBUG_LINE_HEIGHT;
    debugLineCount++;
    
    // Scroll if we reach the bottom
    if (debugY > displayHeight - 50 || debugLineCount > MAX_DEBUG_LINES) {
        // Clear screen and reset
        clearScreen();
        debugY = DEBUG_START_Y;
        debugLineCount = 0;
        
        // Center the scroll header
        gfx->setTextSize(DEBUG_TEXT_SIZE);
        gfx->setTextColor(COLOR_YELLOW);
        const char* scrollMsg = "=== DEBUG LOG ===";
        gfx->getTextBounds(scrollMsg, 0, 0, &x1, &y1, &textWidth, &textHeight);
        centerX = (displayWidth - textWidth) / 2;
        gfx->setCursor(centerX, debugY);
        gfx->println(scrollMsg);
        debugY += DEBUG_LINE_HEIGHT * 2;
    }
}

void DisplayManager::debugPrintf(uint16_t color, const char* format, ...) {
    if (firstImageLoaded) return;
    
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    debugPrint(buffer, color);
}

void DisplayManager::clearDebugArea() {
    debugY = DEBUG_START_Y;
    debugLineCount = 0;
}

void DisplayManager::setFirstImageLoaded(bool loaded) {
    firstImageLoaded = loaded;
}

void DisplayManager::clearScreen(uint16_t color) {
    if (gfx) {
        gfx->fillScreen(color);
    }
}

void DisplayManager::drawBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h) {
    if (gfx && bitmap) {
        gfx->draw16bitRGBBitmap(x, y, bitmap, w, h);
    }
}

void DisplayManager::showSystemStatus() {
    if (!gfx) return;
    
    clearScreen();
    
    gfx->setTextSize(2);
    gfx->setTextColor(COLOR_WHITE);
    gfx->setCursor(10, 10);
    gfx->println("ESP32-P4 AllSky Display");
    
    gfx->setTextSize(1);
    gfx->setTextColor(COLOR_CYAN);
    gfx->setCursor(10, 40);
    gfx->printf("Display: %dx%d pixels\n", displayWidth, displayHeight);
    gfx->printf("Brightness: %d%%\n", displayBrightness);
    gfx->printf("Free Heap: %d bytes\n", systemMonitor.getCurrentFreeHeap());
    gfx->printf("Free PSRAM: %d bytes\n", systemMonitor.getCurrentFreePsram());
    gfx->printf("System Health: %s\n", systemMonitor.isSystemHealthy() ? "HEALTHY" : "CRITICAL");
}
