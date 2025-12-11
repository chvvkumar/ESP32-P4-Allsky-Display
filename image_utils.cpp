#include "image_utils.h"
#include "system_monitor.h"

extern SystemMonitor systemMonitor;  // Defined in main .ino file

bool ImageUtils::softwareTransform(
    const uint16_t* srcBuffer, int srcWidth, int srcHeight,
    uint16_t* dstBuffer, int dstWidth, int dstHeight,
    int rotation
) {
    if (!srcBuffer || !dstBuffer) {
        Serial.println("[ImageUtils] ERROR: NULL buffer pointers");
        return false;
    }
    
    if (srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0) {
        Serial.println("[ImageUtils] ERROR: Invalid dimensions");
        return false;
    }
    
    // For now, only support 0° rotation (scale only)
    // Future enhancement: implement 90°, 180°, 270° rotation
    if (rotation != 0) {
        Serial.printf("[ImageUtils] WARNING: Rotation %d° not yet implemented, using 0°\n", rotation);
    }
    
    Serial.printf("[ImageUtils] Software scaling: %dx%d -> %dx%d\n", 
                 srcWidth, srcHeight, dstWidth, dstHeight);
    
    return bilinearScale(srcBuffer, srcWidth, srcHeight, dstBuffer, dstWidth, dstHeight);
}

bool ImageUtils::bilinearScale(
    const uint16_t* srcBuffer, int srcWidth, int srcHeight,
    uint16_t* dstBuffer, int dstWidth, int dstHeight
) {
    // Calculate scaling ratios
    float xRatio = (float)(srcWidth - 1) / dstWidth;
    float yRatio = (float)(srcHeight - 1) / dstHeight;
    
    Serial.printf("[ImageUtils] Scale ratios: X=%.3f, Y=%.3f\n", xRatio, yRatio);
    
    unsigned long startTime = millis();
    int pixelsProcessed = 0;
    unsigned long lastWatchdogReset = millis();
    
    // Process each destination pixel
    for (int dstY = 0; dstY < dstHeight; dstY++) {
        // Reset watchdog periodically during long operations
        if (millis() - lastWatchdogReset > 100) {
            systemMonitor.forceResetWatchdog();
            lastWatchdogReset = millis();
        }
        
        for (int dstX = 0; dstX < dstWidth; dstX++) {
            // Map destination coordinates to source coordinates
            float srcX = dstX * xRatio;
            float srcY = dstY * yRatio;
            
            // Get integer and fractional parts
            int x0 = (int)srcX;
            int y0 = (int)srcY;
            int x1 = (x0 + 1 < srcWidth) ? x0 + 1 : x0;
            int y1 = (y0 + 1 < srcHeight) ? y0 + 1 : y0;
            
            float xFrac = srcX - x0;
            float yFrac = srcY - y0;
            
            // Get the four surrounding pixels
            uint16_t p00 = srcBuffer[y0 * srcWidth + x0];
            uint16_t p10 = srcBuffer[y0 * srcWidth + x1];
            uint16_t p01 = srcBuffer[y1 * srcWidth + x0];
            uint16_t p11 = srcBuffer[y1 * srcWidth + x1];
            
            // Extract RGB components for all four pixels
            uint8_t r00, g00, b00, r10, g10, b10, r01, g01, b01, r11, g11, b11;
            rgb565ToRgb(p00, r00, g00, b00);
            rgb565ToRgb(p10, r10, g10, b10);
            rgb565ToRgb(p01, r01, g01, b01);
            rgb565ToRgb(p11, r11, g11, b11);
            
            // Bilinear interpolation for each channel
            // Top row interpolation
            float r0 = r00 * (1 - xFrac) + r10 * xFrac;
            float g0 = g00 * (1 - xFrac) + g10 * xFrac;
            float b0 = b00 * (1 - xFrac) + b10 * xFrac;
            
            // Bottom row interpolation
            float r1 = r01 * (1 - xFrac) + r11 * xFrac;
            float g1 = g01 * (1 - xFrac) + g11 * xFrac;
            float b1 = b01 * (1 - xFrac) + b11 * xFrac;
            
            // Vertical interpolation
            uint8_t r = (uint8_t)(r0 * (1 - yFrac) + r1 * yFrac);
            uint8_t g = (uint8_t)(g0 * (1 - yFrac) + g1 * yFrac);
            uint8_t b = (uint8_t)(b0 * (1 - yFrac) + b1 * yFrac);
            
            // Pack and store result
            dstBuffer[dstY * dstWidth + dstX] = rgbToRgb565(r, g, b);
            pixelsProcessed++;
        }
    }
    
    unsigned long duration = millis() - startTime;
    Serial.printf("[ImageUtils] ✓ Software scaling complete: %d pixels in %lu ms (%.1f ms/Kpixel)\n",
                 pixelsProcessed, duration, (duration * 1000.0) / pixelsProcessed);
    
    return true;
}
