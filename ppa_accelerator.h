#pragma once
#ifndef PPA_ACCELERATOR_H
#define PPA_ACCELERATOR_H

#include <Arduino.h>
#include "config.h"

extern "C" {
#include "driver/ppa.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
}

class PPAAccelerator {
private:
    // PPA Hardware Acceleration variables
    ppa_client_handle_t ppa_scaling_handle;
    bool ppa_available;
    uint16_t* ppa_src_buffer;      // DMA-aligned source buffer
    uint16_t* ppa_dst_buffer;      // DMA-aligned destination buffer
    size_t ppa_src_buffer_size;
    size_t ppa_dst_buffer_size;
    
    // Debug function pointer
    void (*debugPrintFunc)(const char* message, uint16_t color);
    void (*debugPrintfFunc)(uint16_t color, const char* format, ...);

public:
    PPAAccelerator();
    ~PPAAccelerator();
    
    // Initialization and cleanup
    bool begin(int16_t displayWidth, int16_t displayHeight);
    void cleanup();
    
    // Hardware acceleration status
    bool isAvailable() const;
    
    // Image processing functions
    bool scaleRotateImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                         uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight, 
                         float rotation = 0.0);
    
    bool scaleImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                   uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight);
    
    // Buffer information
    size_t getSourceBufferSize() const;
    size_t getDestinationBufferSize() const;
    
    // Debug functions
    void setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                          void (*debugPrintf)(uint16_t, const char*, ...));
    
    // Print status information
    void printStatus();

private:
    // Internal helper functions
    ppa_srm_rotation_angle_t convertRotationAngle(float rotation);
    bool validateBufferSizes(size_t srcSize, size_t dstSize);
};

// Global instance
extern PPAAccelerator ppaAccelerator;

#endif // PPA_ACCELERATOR_H
