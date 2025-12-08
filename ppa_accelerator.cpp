#include "ppa_accelerator.h"
#include "logging.h"

// Global instance
PPAAccelerator ppaAccelerator;

PPAAccelerator::PPAAccelerator() :
    ppa_scaling_handle(nullptr),
    ppa_available(false),
    ppa_src_buffer(nullptr),
    ppa_dst_buffer(nullptr),
    ppa_src_buffer_size(0),
    ppa_dst_buffer_size(0),
    debugPrintFunc(nullptr),
    debugPrintfFunc(nullptr)
{
}

PPAAccelerator::~PPAAccelerator() {
    cleanup();
}

bool PPAAccelerator::begin(int16_t displayWidth, int16_t displayHeight) {
    if (debugPrintFunc) debugPrintFunc("Initializing PPA hardware...", COLOR_YELLOW);
    LOG_INFO("Initializing PPA hardware acceleration...");
    
    // Configure PPA client for scaling operations
    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,  // Scaling, Rotating, and Mirror operations
    };
    
    esp_err_t ret = ppa_register_client(&ppa_client_config, &ppa_scaling_handle);
    if (ret != ESP_OK) {
        LOG_ERROR_F("PPA client registration failed: %s\n", esp_err_to_name(ret));
        if (debugPrintfFunc) {
            debugPrintfFunc(COLOR_RED, "PPA client registration failed: %s", esp_err_to_name(ret));
        }
        return false;
    }
    
    // Allocate DMA-aligned buffers for PPA operations
    // Source buffer - dynamically sized based on FULL_IMAGE_BUFFER_SIZE config
    ppa_src_buffer_size = FULL_IMAGE_BUFFER_SIZE; // Matches full image buffer size from config.h
    ppa_src_buffer = (uint16_t*)heap_caps_aligned_alloc(64, ppa_src_buffer_size, 
                                                        MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    
    if (!ppa_src_buffer) {
        LOG_ERROR("ERROR: PPA source buffer allocation failed!");
        if (debugPrintFunc) debugPrintFunc("ERROR: PPA source buffer allocation failed!", COLOR_RED);
        ppa_unregister_client(ppa_scaling_handle);
        return false;
    }
    
    // Destination buffer - dynamically sized based on display and scale multiplier
    ppa_dst_buffer_size = displayWidth * displayHeight * 2 * SCALED_BUFFER_MULTIPLIER; // Matches scaled buffer logic
    ppa_dst_buffer = (uint16_t*)heap_caps_aligned_alloc(64, ppa_dst_buffer_size, 
                                                        MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    
    if (!ppa_dst_buffer) {
        LOG_ERROR("ERROR: PPA destination buffer allocation failed!");
        if (debugPrintFunc) debugPrintFunc("ERROR: PPA destination buffer allocation failed!", COLOR_RED);
        heap_caps_free(ppa_src_buffer);
        ppa_unregister_client(ppa_scaling_handle);
        return false;
    }
    
    ppa_available = true;
    
    LOG_INFO("PPA hardware initialized successfully!");
    LOG_INFO_F("PPA src buffer: %d bytes\n", ppa_src_buffer_size);
    LOG_INFO_F("PPA dst buffer: %d bytes\n", ppa_dst_buffer_size);
    
    if (debugPrintFunc) {
        debugPrintFunc("PPA hardware initialized successfully!", COLOR_GREEN);
        debugPrintfFunc(COLOR_WHITE, "PPA src buffer: %d bytes", ppa_src_buffer_size);
        debugPrintfFunc(COLOR_WHITE, "PPA dst buffer: %d bytes", ppa_dst_buffer_size);
    }
    
    return true;
}

void PPAAccelerator::cleanup() {
    if (ppa_scaling_handle) {
        ppa_unregister_client(ppa_scaling_handle);
        ppa_scaling_handle = nullptr;
    }
    
    if (ppa_src_buffer) {
        heap_caps_free(ppa_src_buffer);
        ppa_src_buffer = nullptr;
    }
    
    if (ppa_dst_buffer) {
        heap_caps_free(ppa_dst_buffer);
        ppa_dst_buffer = nullptr;
    }
    
    ppa_available = false;
}

bool PPAAccelerator::isAvailable() const {
    return ppa_available;
}

bool PPAAccelerator::scaleRotateImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                                     uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight, 
                                     float rotation) {
    if (!ppa_available || !ppa_scaling_handle) {
        LOG_DEBUG("DEBUG: PPA not available or handle invalid");
        return false; // Fall back to software scaling
    }
    
    // Convert rotation angle to PPA enum
    ppa_srm_rotation_angle_t ppa_rotation = convertRotationAngle(rotation);
    if (ppa_rotation == (ppa_srm_rotation_angle_t)-1) {
        LOG_DEBUG_F("DEBUG: Invalid rotation angle: %.1f\n", rotation);
        return false; // Invalid rotation angle
    }
    
    // Check if source and destination fit in PPA buffers
    size_t srcSize = srcWidth * srcHeight * sizeof(uint16_t);
    size_t dstSize = dstWidth * dstHeight * sizeof(uint16_t);
    
    if (!validateBufferSizes(srcSize, dstSize)) {
        return false;
    }
    
    LOG_DEBUG_F("DEBUG: PPA scale+rotate %dx%d -> %dx%d (%.1f°, src:%d dst:%d bytes)\n", 
                 srcWidth, srcHeight, dstWidth, dstHeight, rotation, srcSize, dstSize);
    
    // Copy source data to DMA-aligned buffer
    memcpy(ppa_src_buffer, srcPixels, srcSize);
    
    // Ensure cache coherency for DMA operations
    // Size must be aligned to cache line size (64 bytes)
    size_t srcSizeAligned = (srcSize + 63) & ~63;
    esp_cache_msync(ppa_src_buffer, srcSizeAligned, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    
    // Configure PPA scaling and rotation operation
    ppa_srm_oper_config_t srm_oper_config = {};
    
    // Source configuration
    srm_oper_config.in.buffer = ppa_src_buffer;
    srm_oper_config.in.pic_w = srcWidth;
    srm_oper_config.in.pic_h = srcHeight;
    srm_oper_config.in.block_w = srcWidth;
    srm_oper_config.in.block_h = srcHeight;
    srm_oper_config.in.block_offset_x = 0;
    srm_oper_config.in.block_offset_y = 0;
    srm_oper_config.in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    
    // Output configuration
    srm_oper_config.out.buffer = ppa_dst_buffer;
    srm_oper_config.out.buffer_size = ppa_dst_buffer_size;
    srm_oper_config.out.pic_w = dstWidth;
    srm_oper_config.out.pic_h = dstHeight;
    srm_oper_config.out.block_offset_x = 0;
    srm_oper_config.out.block_offset_y = 0;
    srm_oper_config.out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
    
    // Scaling configuration
    srm_oper_config.scale_x = (float)dstWidth / srcWidth;
    srm_oper_config.scale_y = (float)dstHeight / srcHeight;
    
    // Rotation configuration
    srm_oper_config.rotation_angle = ppa_rotation;
    
    // Mirror configuration (no mirroring)
    srm_oper_config.mirror_x = false;
    srm_oper_config.mirror_y = false;
    
    // Additional configuration
    srm_oper_config.rgb_swap = false;
    srm_oper_config.byte_swap = false;
    srm_oper_config.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    srm_oper_config.mode = PPA_TRANS_MODE_BLOCKING;
    srm_oper_config.user_data = nullptr;
    
    LOG_DEBUG_F("DEBUG: PPA scale factors: x=%.3f, y=%.3f, rotation=%d\n", 
                 srm_oper_config.scale_x, srm_oper_config.scale_y, (int)ppa_rotation);
    
    // Perform the scaling and rotation operation
    esp_err_t ret = ppa_do_scale_rotate_mirror(ppa_scaling_handle, &srm_oper_config);
    if (ret != ESP_OK) {
        LOG_ERROR_F("PPA scale+rotate operation failed: %s (0x%x)\n", esp_err_to_name(ret), ret);
        return false;
    }
    
    // Ensure cache coherency for reading results
    // Size must be aligned to cache line size (64 bytes)
    size_t dstSizeAligned = (dstSize + 63) & ~63;
    esp_cache_msync(ppa_dst_buffer, dstSizeAligned, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    
    // Copy result to destination buffer
    memcpy(dstPixels, ppa_dst_buffer, dstSize);
    
    LOG_DEBUG("DEBUG: PPA scale+rotate successful!");
    return true;
}

bool PPAAccelerator::scaleImage(uint16_t* srcPixels, int16_t srcWidth, int16_t srcHeight,
                               uint16_t* dstPixels, int16_t dstWidth, int16_t dstHeight) {
    return scaleRotateImage(srcPixels, srcWidth, srcHeight, dstPixels, dstWidth, dstHeight, 0.0);
}

size_t PPAAccelerator::getSourceBufferSize() const {
    return ppa_src_buffer_size;
}

size_t PPAAccelerator::getDestinationBufferSize() const {
    return ppa_dst_buffer_size;
}

void PPAAccelerator::setDebugFunctions(void (*debugPrint)(const char*, uint16_t), 
                                       void (*debugPrintf)(uint16_t, const char*, ...)) {
    debugPrintFunc = debugPrint;
    debugPrintfFunc = debugPrintf;
}

void PPAAccelerator::printStatus() {
    LOG_INFO("=== PPA Hardware Accelerator Status ===");
    LOG_INFO_F("Available: %s\n", ppa_available ? "YES" : "NO");
    if (ppa_available) {
        LOG_INFO_F("Source Buffer: %d bytes\n", ppa_src_buffer_size);
        LOG_INFO_F("Destination Buffer: %d bytes\n", ppa_dst_buffer_size);
        LOG_INFO_F("Handle: %p\n", ppa_scaling_handle);
    }
    LOG_INFO("======================================");
}

ppa_srm_rotation_angle_t PPAAccelerator::convertRotationAngle(float rotation) {
    if (rotation == 0.0) {
        return PPA_SRM_ROTATION_ANGLE_0;
    } else if (rotation == 90.0) {
        return PPA_SRM_ROTATION_ANGLE_90;
    } else if (rotation == 180.0) {
        return PPA_SRM_ROTATION_ANGLE_180;
    } else if (rotation == 270.0) {
        return PPA_SRM_ROTATION_ANGLE_270;
    } else {
        return (ppa_srm_rotation_angle_t)-1; // Invalid
    }
}

bool PPAAccelerator::validateBufferSizes(size_t srcSize, size_t dstSize) {
    if (srcSize > ppa_src_buffer_size) {
        LOG_DEBUG_F("DEBUG: Source too large for PPA (%d > %d)\n", srcSize, ppa_src_buffer_size);
        return false;
    }
    
    if (dstSize > ppa_dst_buffer_size) {
        LOG_DEBUG_F("DEBUG: Destination too large for PPA (%d > %d)\n", dstSize, ppa_dst_buffer_size);
        return false;
    }
    
    return true;
}
