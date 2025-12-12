#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <Arduino.h>

/**
 * @brief Image processing utilities for software-based transformations
 * 
 * Provides fallback image scaling and rotation when hardware PPA is unavailable.
 * Uses bilinear interpolation for smooth scaling results.
 */
class ImageUtils {
public:
    /**
     * @brief Software-based image scaling with optional rotation
     * 
     * @param srcBuffer Source image buffer (RGB565)
     * @param srcWidth Source image width
     * @param srcHeight Source image height
     * @param dstBuffer Destination buffer (must be pre-allocated)
     * @param dstWidth Desired output width
     * @param dstHeight Desired output height
     * @param rotation Rotation angle in degrees (0, 90, 180, 270)
     * @return true if transformation succeeded
     */
    static bool softwareTransform(
        const uint16_t* srcBuffer, int srcWidth, int srcHeight,
        uint16_t* dstBuffer, int dstWidth, int dstHeight,
        int rotation = 0
    );
    
    /**
     * @brief Bilinear interpolation scaling (no rotation)
     * 
     * @param srcBuffer Source image buffer (RGB565)
     * @param srcWidth Source image width
     * @param srcHeight Source image height
     * @param dstBuffer Destination buffer (must be pre-allocated)
     * @param dstWidth Desired output width
     * @param dstHeight Desired output height
     * @return true if scaling succeeded
     */
    static bool bilinearScale(
        const uint16_t* srcBuffer, int srcWidth, int srcHeight,
        uint16_t* dstBuffer, int dstWidth, int dstHeight
    );

private:
    /**
     * @brief Extract RGB components from RGB565 color
     */
    static inline void rgb565ToRgb(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
        r = (color >> 11) & 0x1F;
        g = (color >> 5) & 0x3F;
        b = color & 0x1F;
    }
    
    /**
     * @brief Pack RGB components into RGB565 color
     */
    static inline uint16_t rgbToRgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
    }
};

#endif // IMAGE_UTILS_H
