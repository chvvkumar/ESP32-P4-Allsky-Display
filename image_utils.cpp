#include "image_utils.h"
#include "system_monitor.h"

extern SystemMonitor systemMonitor;  // Defined in main .ino file

void ImageUtils::getKelvinScales(int temp, float& rScale, float& gScale, float& bScale) {
    // 6500K is neutral (1.0, 1.0, 1.0)
    // Simplified approximation for performance (Tanner Helland's method)
    temp = constrain(temp, 1000, 40000) / 100;
    
    // Calculate Red
    if (temp <= 66) {
        rScale = 1.0f;
    } else {
        rScale = constrain(pow(temp - 60, -0.1332047592) * 329.698727446 / 255.0, 0.0, 1.0);
    }
    
    // Calculate Green
    if (temp <= 66) {
        gScale = constrain(log(temp) * 99.4708025861 - 161.1195681661, 0.0, 255.0) / 255.0;
    } else {
        gScale = constrain(pow(temp - 60, -0.0755148492) * 288.1221695283 / 255.0, 0.0, 1.0);
    }
    
    // Calculate Blue
    if (temp >= 66) {
        bScale = 1.0f;
    } else if (temp <= 19) {
        bScale = 0.0f;
    } else {
        bScale = constrain(log(temp - 10) * 138.5177312231 - 305.0447927307, 0.0, 255.0) / 255.0;
    }
}

void ImageUtils::adjustColorTemperature(uint16_t* buffer, int width, int height, int tempKelvin) {
    if (tempKelvin == 6500) return; // Neutral, no-op

    float rS, gS, bS;
    getKelvinScales(tempKelvin, rS, gS, bS);

    // Use fixed-point math for speed (scale by 256)
    int rFix = rS * 256;
    int gFix = gS * 256;
    int bFix = bS * 256;

    int len = width * height;
    for (int i = 0; i < len; i++) {
        uint16_t p = buffer[i];
        
        // Unpack RGB565
        int r = (p >> 11) & 0x1F;
        int g = (p >> 5) & 0x3F;
        int b = p & 0x1F;

        // Apply scaling
        r = constrain((r * rFix) >> 8, 0, 31);
        g = constrain((g * gFix) >> 8, 0, 63);
        b = constrain((b * bFix) >> 8, 0, 31);

        // Repack
        buffer[i] = (r << 11) | (g << 5) | b;
    }
}

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
    // 16.16 fixed-point arithmetic for ~2-4x speedup over float on RISC-V
    static constexpr int FP_SHIFT = 16;
    static constexpr uint32_t FP_ONE = 1u << FP_SHIFT;
    static constexpr uint32_t FP_MASK = FP_ONE - 1;

    // Calculate scaling ratios in 16.16 fixed-point
    uint32_t xRatio_fp = ((uint32_t)(srcWidth - 1) << FP_SHIFT) / dstWidth;
    uint32_t yRatio_fp = ((uint32_t)(srcHeight - 1) << FP_SHIFT) / dstHeight;

    Serial.printf("[ImageUtils] Scale ratios (fixed-point): X=0x%08X, Y=0x%08X\n", xRatio_fp, yRatio_fp);

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

        uint32_t srcY_fp = (uint32_t)dstY * yRatio_fp;
        int y0 = srcY_fp >> FP_SHIFT;
        uint32_t yFrac = srcY_fp & FP_MASK;
        uint32_t yFracInv = FP_ONE - yFrac;
        int y1 = (y0 + 1 < srcHeight) ? y0 + 1 : y0;

        const uint16_t* srcRow0 = srcBuffer + y0 * srcWidth;
        const uint16_t* srcRow1 = srcBuffer + y1 * srcWidth;

        for (int dstX = 0; dstX < dstWidth; dstX++) {
            uint32_t srcX_fp = (uint32_t)dstX * xRatio_fp;
            int x0 = srcX_fp >> FP_SHIFT;
            uint32_t xFrac = srcX_fp & FP_MASK;
            uint32_t xFracInv = FP_ONE - xFrac;
            int x1 = (x0 + 1 < srcWidth) ? x0 + 1 : x0;

            // Get the four surrounding pixels
            uint16_t p00 = srcRow0[x0];
            uint16_t p10 = srcRow0[x1];
            uint16_t p01 = srcRow1[x0];
            uint16_t p11 = srcRow1[x1];

            // Extract RGB565 components inline
            uint32_t r00 = (p00 >> 11) & 0x1F;
            uint32_t g00 = (p00 >> 5) & 0x3F;
            uint32_t b00 = p00 & 0x1F;

            uint32_t r10 = (p10 >> 11) & 0x1F;
            uint32_t g10 = (p10 >> 5) & 0x3F;
            uint32_t b10 = p10 & 0x1F;

            uint32_t r01 = (p01 >> 11) & 0x1F;
            uint32_t g01 = (p01 >> 5) & 0x3F;
            uint32_t b01 = p01 & 0x1F;

            uint32_t r11 = (p11 >> 11) & 0x1F;
            uint32_t g11 = (p11 >> 5) & 0x3F;
            uint32_t b11 = p11 & 0x1F;

            // Bilinear interpolation using fixed-point (all in 16.16)
            // Top row: lerp(p00, p10, xFrac)
            uint32_t r0 = r00 * xFracInv + r10 * xFrac;
            uint32_t g0 = g00 * xFracInv + g10 * xFrac;
            uint32_t b0 = b00 * xFracInv + b10 * xFrac;

            // Bottom row: lerp(p01, p11, xFrac)
            uint32_t r1 = r01 * xFracInv + r11 * xFrac;
            uint32_t g1 = g01 * xFracInv + g11 * xFrac;
            uint32_t b1 = b01 * xFracInv + b11 * xFrac;

            // Vertical: lerp(row0, row1, yFrac) - result is in 32.16 after two multiplies
            // r0/g0/b0 are already in 5.16 (or 6.16 for green), shift down by 2*FP_SHIFT
            uint32_t r = (r0 * yFracInv + r1 * yFrac) >> (2 * FP_SHIFT);
            uint32_t g = (g0 * yFracInv + g1 * yFrac) >> (2 * FP_SHIFT);
            uint32_t b = (b0 * yFracInv + b1 * yFrac) >> (2 * FP_SHIFT);

            // Pack RGB565 and store
            dstBuffer[dstY * dstWidth + dstX] = ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
            pixelsProcessed++;
        }
    }

    unsigned long duration = millis() - startTime;
    Serial.printf("[ImageUtils] Software scaling complete: %d pixels in %lu ms (%.1f ms/Kpixel)\n",
                 pixelsProcessed, duration, (duration * 1000.0) / pixelsProcessed);

    return true;
}
