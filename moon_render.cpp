#include "moon_render.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

// 4x4 Bayer ordered-dither threshold matrix (0..15).
static const uint8_t s_bayer4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

static inline uint16_t rgb565c(int r, int g, int b) {
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Flat fallback: no surface texture yet (Task 10 replaces this).
static inline void sampleTex(float nx, float ny, int* lum, int* alpha) {
    (void)nx; (void)ny;
    *lum = 200; *alpha = 255;
}

uint16_t* moonRender(int w, int h, const moon_state_t* st, uint8_t bg_style) {
    uint16_t* buf = (uint16_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (!buf) { Serial.println("[moon] PSRAM render alloc failed"); return nullptr; }

    // Single-precision throughout: the ESP32-P4 FPU is float-only.
    const float R  = (w < h ? w : h) * 0.5f * 0.92f;
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const float AA = 1.5f;
    const float f  = st->illum;
    const int   waxing = st->waxing ? 1 : 0;
    const float ca = cosf(st->orient_rad), sa = sinf(st->orient_rad);
    const float R2 = R * R;
    const float tint_r = 1.02f, tint_g = 0.97f, tint_b = 0.86f;
    const float dk = 0.07f;

    memset(buf, 0, (size_t)w * h * 2);
    if (bg_style == 1 || bg_style == 3) {
        uint32_t seed = 1234567u;
        for (int i = 0; i < (w*h)/900; i++) {
            seed = seed*1103515245u + 12345u; int sx = (seed >> 8) % w;
            seed = seed*1103515245u + 12345u; int sy = (seed >> 8) % h;
            int br = 120 + ((seed >> 4) & 0x7F);
            buf[sy*w + sx] = rgb565c(br, br, br);
        }
    }

    for (int py = 0; py < h; py++) {
        float dy = (float)py - cy;
        uint16_t* row = buf + (size_t)py * w;
        for (int px = 0; px < w; px++) {
            float dx = (float)px - cx;
            float r2 = dx*dx + dy*dy;
            if (r2 > R2) continue;
            float dist = sqrtf(r2);
            float edge = R - dist;
            float edgeA = edge > AA ? 1.0f : edge / AA;

            float rx = ( dx*ca + dy*sa);
            float ry = (-dx*sa + dy*ca);

            float semiArg = R2 - ry*ry;
            float semi = semiArg > 0.0f ? sqrtf(semiArg) : 0.0f;
            float termX = (1.0f - 2.0f*f) * semi;
            float sd = waxing ? (rx - termX) : (-rx - termX);
            float l = sd / AA;
            if (l < 0.0f) l = 0.0f;
            if (l > 1.0f) l = 1.0f;

            int lum, alpha;
            sampleTex(rx / R, ry / R, &lum, &alpha);
            float limb = 1.0f - 0.45f * (r2 / R2);
            float litR = lum * tint_r * limb;
            float litG = lum * tint_g * limb;
            float litB = lum * tint_b * limb;
            int r = (int)(l*litR + (1.0f-l)*litR*dk);
            int g = (int)(l*litG + (1.0f-l)*litG*dk);
            int b = (int)(l*litB + (1.0f-l)*litB*dk + (1.0f-l)*4.0f);

            if (edgeA < 1.0f) { r = (int)(r*edgeA); g = (int)(g*edgeA); b = (int)(b*edgeA); }
            row[px] = rgb565c(r, g, b);
        }
    }

    if (bg_style == 2 || bg_style == 3) {
        const float Rout2 = (R * 1.35f) * (R * 1.35f);
        for (int py = 0; py < h; py++) {
            float dy = (float)py - cy;
            uint16_t* row = buf + (size_t)py * w;
            for (int px = 0; px < w; px++) {
                float dx = (float)px - cx;
                float r2 = dx*dx + dy*dy;
                if (r2 <= R2 || r2 >= Rout2) continue;
                float dist = sqrtf(r2);
                float t = 1.0f - (dist - R) / (R*0.35f);
                int add = (int)(40.0f * t);
                uint16_t c = row[px];
                int r = ((c>>11)&0x1F)*255/31 + add;
                int g = ((c>>5)&0x3F)*255/63 + add*9/10;
                int b = (c&0x1F)*255/31 + add*7/10;
                int dr = s_bayer4[py & 3][px & 3] >> 1;
                int dg = s_bayer4[py & 3][px & 3] >> 2;
                int db = dr;
                row[px] = rgb565c(r + dr, g + dg, b + db);
            }
        }
    }
    return buf;
}
