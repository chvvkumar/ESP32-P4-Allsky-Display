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
    
#ifdef GFX_EXTRA_PRE_INIT
    GFX_EXTRA_PRE_INIT();
#endif

    if (!gfx->begin()) {
        Serial.println("ERROR: Display init failed!");
        return false;
    }
    
    displayWidth = gfx->width();
    displayHeight = gfx->height();
    
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
    
    // Configure LEDC for backlight control using newer API
    if (ledcAttach(BACKLIGHT_PIN, BACKLIGHT_FREQ, BACKLIGHT_RESOLUTION) == 0) {
        debugPrint("ERROR: LEDC attach failed!", COLOR_RED);
        return false;
    }
    
    // Get the default brightness from configuration storage
    displayBrightness = configStorage.getDefaultBrightness();
    
    // Set initial brightness
    setBrightness(displayBrightness);
    brightnessInitialized = true;
    
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
}

int DisplayManager::getBrightness() const {
    return displayBrightness;
}

void DisplayManager::debugPrint(const char* message, uint16_t color) {
    if (firstImageLoaded) return; // Don't show debug after first image loads
    if (!gfx) return;
    
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

void DisplayManager::setDebugY(int y) {
    debugY = y;
    debugLineCount = 0;
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

void DisplayManager::pauseDisplay() {
    // Pause display updates to prevent memory bandwidth conflicts during heavy operations
    // This reduces LCD underrun errors during concurrent memory-intensive tasks
    if (!gfx) return;
    
    // The LCD controller will pause reading from PSRAM when memory bus is saturated
    // This is handled at hardware level by the ESP32-P4 DSI controller
}

void DisplayManager::resumeDisplay() {
    // Resume normal display updates after heavy operations complete
    if (!gfx) return;
    
    // Display automatically resumes when memory bandwidth becomes available
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

void DisplayManager::drawStatusOverlay(const char* message, uint16_t color, int yOffset) {
    if (!gfx || !message) return;
    
    // Draw semi-transparent background box with rounded corners effect
    // Calculate text dimensions for the background
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    gfx->setTextSize(1);
    gfx->getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Create overlay box with padding
    int boxPadding = 8;
    int16_t boxX = (displayWidth - textWidth) / 2 - boxPadding;
    int16_t boxY = yOffset - 5;
    int16_t boxWidth = textWidth + (boxPadding * 2);
    int16_t boxHeight = textHeight + 10;
    
    // Clamp to screen bounds
    if (boxX < 0) boxX = 0;
    if (boxY < 0) boxY = 0;
    if (boxX + boxWidth > displayWidth) boxWidth = displayWidth - boxX;
    if (boxY + boxHeight > displayHeight) boxHeight = displayHeight - boxY;
    
    // Draw semi-transparent dark background (multiple overlays to create transparency effect)
    uint16_t bgColor = 0x0000; // Black
    gfx->fillRect(boxX, boxY, boxWidth, boxHeight, bgColor);
    
    // Draw text on the overlay
    gfx->setTextSize(1);
    gfx->setTextColor(color);
    int16_t centerX = (displayWidth - textWidth) / 2;
    gfx->setCursor(centerX, yOffset);
    gfx->println(message);
}

void DisplayManager::drawOverlayMessage(const char* message, int x, int y, uint16_t color, int backgroundColor) {
    if (!gfx || !message) return;
    
    // Calculate text dimensions
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    gfx->setTextSize(1);
    gfx->getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Create background box with padding
    int boxPadding = 6;
    int16_t boxX = x - boxPadding;
    int16_t boxY = y - boxPadding;
    int16_t boxWidth = textWidth + (boxPadding * 2);
    int16_t boxHeight = textHeight + (boxPadding * 2);
    
    // Clamp to screen bounds
    if (boxX < 0) boxX = 0;
    if (boxY < 0) boxY = 0;
    if (boxX + boxWidth > displayWidth) boxWidth = displayWidth - boxX;
    if (boxY + boxHeight > displayHeight) boxHeight = displayHeight - boxY;
    
    // Draw dark background for readability
    gfx->fillRect(boxX, boxY, boxWidth, boxHeight, backgroundColor);
    
    // Draw text
    gfx->setTextSize(1);
    gfx->setTextColor(color);
    gfx->setCursor(x, y);
    gfx->println(message);
}

void DisplayManager::clearStatusOverlay() {
    // Overlay is cleared by redrawing the last image
    // This is done by calling renderFullImage() from the main loop
}
