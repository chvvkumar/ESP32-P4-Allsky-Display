#pragma once
#ifndef MOON_RENDER_H
#define MOON_RENDER_H

#include "moon_ephemeris.h"
#include <stdint.h>

// Render a moon disc into a freshly-allocated PSRAM RGB565 buffer of size w*h
// (caller frees with heap_caps_free), or nullptr on failure.
// bg_style: 0=black, 1=starfield, 2=glow, 3=stars+glow.
uint16_t* moonRender(int w, int h, const moon_state_t* st, uint8_t bg_style);

#endif // MOON_RENDER_H
