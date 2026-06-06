// image_presets.h
#pragma once
#ifndef IMAGE_PRESETS_H
#define IMAGE_PRESETS_H

#include <Arduino.h>

// A selectable full-disc image source. label is shown in the web UI;
// url is added verbatim as a normal image source. cropPct is the
// recommended fraction of the source kept after a centered crop (trims
// black margin / burned-in timestamps); 100 = no crop.
struct ImagePreset {
    const char* id;       // stable key used by the web API
    const char* label;    // human-readable name
    const char* url;      // JPEG-over-HTTPS source
    uint16_t    nominalPx; // expected source width/height (square), for fit scaling
    uint8_t     cropPct;  // 80..100
};

extern const ImagePreset IMAGE_PRESETS[];
extern const int IMAGE_PRESET_COUNT;

// Returns the preset with matching id, or nullptr.
const ImagePreset* findImagePreset(const char* id);

#endif // IMAGE_PRESETS_H
