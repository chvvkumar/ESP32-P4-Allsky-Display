#pragma once
#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "displays_config.h"
#include "config.h"

class DisplayManager {
private:
    // Display objects
    Arduino_ESP32DSIPanel* dsipanel;
    Arduino_DSI_Display* gfx;
    
    // Display dimensions
    int16_t displayWidth;
    int16_t displayHeight;
    
    // Brightness control
    int displayBrightness;
    bool brightnessInitialized;
    
    // Debug display variables
    int debugY;
    int debugLineCount;
    bool firstImageLoaded;

public:
    DisplayManager();
    ~DisplayManager();
    
    // Initialization
    bool begin();
    void cleanup();
    
    // Display access
    Arduino_DSI_Display* getGFX() const;
    int16_t getWidth() const;
    int16_t getHeight() const;
    
    // Brightness control
    bool initBrightness();
    void setBrightness(int brightness);
    int getBrightness() const;
    
    // Debug output functions
    void debugPrint(const char* message, uint16_t color = COLOR_WHITE);
    void debugPrintf(uint16_t color, const char* format, ...);
    void clearDebugArea();
    void setFirstImageLoaded(bool loaded);
    
    // Screen operations
    void clearScreen(uint16_t color = COLOR_BLACK);
    void drawBitmap(int16_t x, int16_t y, uint16_t* bitmap, int16_t w, int16_t h);
    
    // Status display
    void showSystemStatus();
};

// Global instance
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
