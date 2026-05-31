# Solar and Lunar Full-Disc Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add NASA/SOHO/GOES solar full-disc imagery as preset image sources, and an on-device computed Moon-phase image source, both rendered centered on the round DSI panel using the existing per-image transform system.

**Architecture:** Solar imagery is plain JPEG-over-HTTPS and reuses the existing multi-source download/decode/cycle pipeline unchanged; it is delivered as a preset URL picker in the web UI, and each added source keeps its own independent scale/offset/rotation via the existing `imageTransforms[10]` array. The Moon is not downloaded: a ported Schlyter ephemeris computes phase/orientation, and a per-pixel RGB565 renderer (ported from the NINA project) draws the lit disc directly into the existing `pendingFullImageBuffer`, triggered by a sentinel `moon://` source URL that branches inside `downloadAndDisplayImage()` before the HTTP path.

**Tech Stack:** Arduino core for ESP32-P4, JPEGDEC, Arduino_GFX (direct DSI), HTTPClient/WiFiClientSecure, Preferences (NVS), FreeRTOS. Pure-C ephemeris math (`math.h` only). Host-side unit test via `gcc`/`cc`.

**Source references (verified):**
- Download entry: `ESP32-P4-Allsky-Display.ino:1061` `downloadAndDisplayImage()`; URL chosen at `:1092` via `getCurrentImageURL()` (`:1732`).
- Decode writes `pendingFullImageBuffer`, sets `imageReadyToDisplay=true` at `:1542`; buffer swap + `renderFullImage()` at `:2078`.
- Per-image transforms loaded into globals `scaleX/scaleY/offsetX/offsetY/rotationAngle` by `updateCurrentImageTransformSettings()` (`:1666`), backed by `ConfigStorage::getImageScaleX/...` and the private `imageTransforms[10]` struct (`config_storage.h:242-289`).
- Web source handlers: `web_config_api.cpp:347` `handleAddImageSource()`, `:385` `handleUpdateImageSource()`.
- Moon source to port: `ESP32-P4-NINA-Display/main/moon_ephemeris.{c,h}` (pure math) and `moon_render.{c,h}` (per-pixel RGB565). Texture `moon_texture.png` is 500x500 RGBA.
- Build: `compile-and-upload.ps1 -SkipUpload`, FQBN `esp32:esp32:esp32p4:FlashSize=32M,PartitionScheme=app13M_data7M_32MB,PSRAM=enabled`.

**Conventions to follow:**
- Flat Arduino sketch layout: new units are sibling `.h/.cpp` files in the project root (mirrors `image_utils.*`, `displays_config.*`).
- Large PSRAM buffers via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` / `heap_caps_aligned_alloc`.
- `Serial.printf` logging in the existing style; reset watchdog around long loops with `systemMonitor.forceResetWatchdog()`.
- Per-group NVS dirty flags (`markDirty(DIRTY_*)`); reuse `DIRTY_IMAGE` / `DIRTY_TRANSFORMS` / add one new group for moon settings.

---

## Phase 1: Solar full-disc preset sources

Each solar source is added as a normal image source, so it automatically gets its own entry in `imageSources[]`, `imageEnabled[]`, `imageDurations[]`, and `imageTransforms[]`. No change to the download/decode/render path is required. Phase 1 adds (a) a preset table, (b) a web UI picker that seeds a sensible per-source "fit disc" transform, and (c) on-device HTTPS verification to the NASA/SOHO hosts.

### Task 1: Solar/satellite preset table

**Files:**
- Create: `image_presets.h`
- Create: `image_presets.cpp`

- [ ] **Step 1: Create the preset header**

```cpp
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
```

- [ ] **Step 2: Create the preset table (verified public URLs, no API key)**

```cpp
// image_presets.cpp
#include "image_presets.h"
#include <string.h>

const ImagePreset IMAGE_PRESETS[] = {
    // id,                  label,                         url,                                                                       nominalPx, cropPct
    { "sdo_aia_304",  "Sun SDO/AIA 304A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0304.jpg",                 1024, 88 },
    { "sdo_aia_171",  "Sun SDO/AIA 171A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0171.jpg",                 1024, 88 },
    { "sdo_aia_193",  "Sun SDO/AIA 193A",   "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_1024_0193.jpg",                 1024, 88 },
    { "sdo_hmi_igr",  "Sun SDO/HMI Cont.",  "https://soho.nascom.nasa.gov/data/realtime/hmi_igr/1024/latest.jpg",               1024, 92 },
    { "sdo_hmi_mag",  "Sun SDO/HMI Magn.",  "https://soho.nascom.nasa.gov/data/realtime/hmi_mag/1024/latest.jpg",               1024, 92 },
    { "soho_c2",      "Sun SOHO LASCO C2",  "https://soho.nascom.nasa.gov/data/realtime/c2/1024/latest.jpg",                    1024, 90 },
    { "soho_c3",      "Sun SOHO LASCO C3",  "https://soho.nascom.nasa.gov/data/realtime/c3/1024/latest.jpg",                    1024, 90 },
    { "goes19_full",  "Earth GOES-19",      "https://cdn.star.nesdis.noaa.gov/GOES19/ABI/FD/GEOCOLOR/1808x1808.jpg",            1808, 96 },
};
const int IMAGE_PRESET_COUNT = (int)(sizeof(IMAGE_PRESETS) / sizeof(IMAGE_PRESETS[0]));

const ImagePreset* findImagePreset(const char* id) {
    if (!id) return nullptr;
    for (int i = 0; i < IMAGE_PRESET_COUNT; i++) {
        if (strcmp(IMAGE_PRESETS[i].id, id) == 0) return &IMAGE_PRESETS[i];
    }
    return nullptr;
}
```

- [ ] **Step 3: Build to verify the new unit compiles**

Run: `pwsh -File .\compile-and-upload.ps1 -SkipUpload`
Expected: build reaches "compiling" of the sketch with no error referencing `image_presets`. (First clean build is slow; watch for `error:` lines.)

- [ ] **Step 4: Commit**

```bash
git add image_presets.h image_presets.cpp
git commit -m "feat: add solar/satellite full-disc preset table"
```

### Task 2: Web API endpoint to add a preset (seeds per-source fit transform)

**Files:**
- Modify: `web_config_api.cpp` (add `handleAddPreset()` near `handleAddImageSource()` at `:347`)
- Modify: `web_config.h` (declare `void handleAddPreset();`)
- Modify: `web_config.cpp` (register route `/api/addPreset`)

- [ ] **Step 1: Declare the handler in `web_config.h`**

Find the line declaring `void handleAddImageSource();` and add directly below it:

```cpp
    void handleAddPreset();
```

- [ ] **Step 2: Implement the handler in `web_config_api.cpp`**

Add after `handleAddImageSource()` (which ends at `:364`). It adds the preset URL as a normal source, then seeds that new source's own transform so the disc fits the round panel (scale = displaySize / nominalPx) with the others left at defaults. This satisfies "each solar source has its own scale/transform".

```cpp
#include "image_presets.h"   // add with the other includes at the top of the file

void WebConfig::handleAddPreset() {
    if (!server->hasArg("id")) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"id parameter required\"}");
        return;
    }
    const ImagePreset* p = findImagePreset(server->arg("id").c_str());
    if (!p) {
        sendResponse(404, "application/json", "{\"status\":\"error\",\"message\":\"Unknown preset id\"}");
        return;
    }

    int newIndex = configStorage.getImageSourceCount();      // index the new source will occupy
    if (newIndex >= MAX_IMAGE_SOURCES) {
        sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Maximum image sources reached\"}");
        return;
    }

    configStorage.addImageSource(String(p->url));

    // Seed a "fit disc" transform for THIS source only.
    // Display is square; pick the active panel size and fit the cropped disc to it.
    int displaySize = displayManager.getWidth();             // 800 or 1448 (square panel)
    float keptPx = (float)p->nominalPx * (float)p->cropPct / 100.0f;
    float fit = (keptPx > 0.0f) ? ((float)displaySize / keptPx) : 1.0f;
    if (fit < MIN_SCALE) fit = MIN_SCALE;
    if (fit > MAX_SCALE) fit = MAX_SCALE;
    configStorage.setImageScaleX(newIndex, fit);
    configStorage.setImageScaleY(newIndex, fit);
    configStorage.setImageOffsetX(newIndex, 0);
    configStorage.setImageOffsetY(newIndex, 0);
    configStorage.setImageRotation(newIndex, 0.0f);
    configStorage.saveConfig();

    LOG_INFO_F("[WebAPI] Added preset %s as source %d (fit scale %.2f)\n", p->id, newIndex, fit);
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Preset added\"}");
}
```

- [ ] **Step 3: Register the route in `web_config.cpp`**

Find where `/api/addImageSource` is registered (search for `addImageSource`) and add an adjacent registration in the same style. Match the existing lambda/handler-binding pattern used on that line; the registration looks like:

```cpp
    server->on("/api/addPreset", HTTP_POST, [this]() { handleAddPreset(); });
```

(If the existing routes use `HTTP_GET` or a different binding form, copy that exact form instead.)

- [ ] **Step 4: Build to verify**

Run: `pwsh -File .\compile-and-upload.ps1 -SkipUpload`
Expected: no `error:` lines; `handleAddPreset` resolves.

- [ ] **Step 5: Commit**

```bash
git add web_config_api.cpp web_config.h web_config.cpp
git commit -m "feat: add /api/addPreset endpoint seeding per-source fit transform"
```

### Task 3: Web UI preset picker

**Files:**
- Modify: `web_config_html.h` (the image-sources section of the config page)

- [ ] **Step 1: Add a preset dropdown + button near the existing "Add image source" control**

Locate the image-sources block in `web_config_html.h` (search for the existing add-source input/button that posts to `/api/addImageSource`). Add this markup directly above it:

```html
<div class="preset-row">
  <label for="presetSelect">Add full-disc preset:</label>
  <select id="presetSelect">
    <option value="sdo_aia_304">Sun SDO/AIA 304A</option>
    <option value="sdo_aia_171">Sun SDO/AIA 171A</option>
    <option value="sdo_aia_193">Sun SDO/AIA 193A</option>
    <option value="sdo_hmi_igr">Sun SDO/HMI Continuum</option>
    <option value="sdo_hmi_mag">Sun SDO/HMI Magnetogram</option>
    <option value="soho_c2">Sun SOHO LASCO C2</option>
    <option value="soho_c3">Sun SOHO LASCO C3</option>
    <option value="goes19_full">Earth GOES-19 Full Disc</option>
  </select>
  <button type="button" onclick="addPreset()">Add Preset</button>
</div>
```

- [ ] **Step 2: Add the `addPreset()` script**

In the page's `<script>` section (match how existing buttons call the API; reuse the existing `fetch`/reload helper if one is present), add:

```javascript
function addPreset() {
  const id = document.getElementById('presetSelect').value;
  fetch('/api/addPreset', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'id=' + encodeURIComponent(id)
  })
  .then(r => r.json())
  .then(j => { alert(j.message); location.reload(); })
  .catch(e => alert('Add preset failed: ' + e));
}
```

(If the page already centralizes API calls in a helper, call that helper instead of raw `fetch`, matching the existing style.)

- [ ] **Step 3: Build to verify the HTML header still compiles (string literal sizes)**

Run: `pwsh -File .\compile-and-upload.ps1 -SkipUpload`
Expected: no error. Large PROGMEM string additions occasionally trip line-length limits; if so, split the string literal following the existing pattern in the file.

- [ ] **Step 4: Commit**

```bash
git add web_config_html.h
git commit -m "feat: add full-disc preset picker to config web UI"
```

### Task 4: On-device HTTPS verification to NASA/SOHO/NOAA hosts

The existing defaults already include `https://i.imgur.com/...`, so `http.begin(url)` handles TLS. This task confirms the specific solar hosts succeed, and adds an insecure-TLS fallback only if a host fails certificate validation.

**Files:**
- Modify (only if needed): `ESP32-P4-Allsky-Display.ino` around `http.begin(imageURL)` (`:1125`)

- [ ] **Step 1: Flash current build and add an SDO preset via the web UI**

Run: `pwsh -File .\compile-and-upload.ps1` (uploads; adjust `-ComPort` if not COM3)
Then in the device web UI add the "Sun SDO/AIA 304A" preset and let it cycle to it.

- [ ] **Step 2: Observe the serial log for the fetch result**

Expected on success: `[Image] HTTP response code: 200` followed by a successful JPEG decode and `Buffer swap complete`. The Sun disc should appear centered.
Failure signatures to watch for: response code `-1` / TLS handshake errors in the log.

- [ ] **Step 3 (conditional): Add insecure-TLS fallback if a host fails handshake**

Only if Step 2 shows a TLS failure for an `https://` solar host, replace the single-arg begin at `:1125` with an explicit secure client:

```cpp
// near the top includes of the .ino, if not already present:
#include <WiFiClientSecure.h>

// replace: beginResult = http.begin(imageURL);
static WiFiClientSecure s_secureClient;
auto beginOperation = [&]() {
    if (imageURL.startsWith("https://")) {
        s_secureClient.setInsecure();      // public imagery; skip cert pinning
        beginResult = http.begin(s_secureClient, imageURL);
    } else {
        beginResult = http.begin(imageURL);
    }
    beginCompleted = true;
};
```

Re-run Steps 1-2 to confirm the disc loads.

- [ ] **Step 4: Commit (only if code changed)**

```bash
git add ESP32-P4-Allsky-Display.ino
git commit -m "fix: explicit WiFiClientSecure for HTTPS full-disc sources"
```

> If no code change was needed, record the verification result in the PR description instead.

---

## Phase 2: Moon-phase computed source

The Moon is rendered locally and injected into the existing pipeline through a sentinel source URL `moon://default`. Cycling, per-source transforms, the buffer swap, and `renderFullImage()` all treat it as a normal source.

### Task 5: Port the Schlyter ephemeris (pure math) with a host unit test

**Files:**
- Create: `moon_ephemeris.h` (copy verbatim from `ESP32-P4-NINA-Display/main/moon_ephemeris.h`)
- Create: `moon_ephemeris.c` (copy verbatim from `ESP32-P4-NINA-Display/main/moon_ephemeris.c`)
- Create: `test/test_moon_ephemeris.c` (host test)

- [ ] **Step 1: Copy the two ephemeris files into the project root unchanged**

They depend only on `math.h` and the `moon_state_t` struct. Copy `moon_ephemeris.h` and `moon_ephemeris.c` verbatim. (`extern "C"` guards are already present, so the C file links cleanly into the C++ sketch.)

- [ ] **Step 2: Write the failing host test**

```c
// test/test_moon_ephemeris.c
#include "../moon_ephemeris.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// time_t (UTC) for a date: uses timegm-equivalent via a fixed table is overkill;
// these epochs are precomputed UTC seconds for known lunar events.
// 2025-01-13 22:27 UTC = Full Moon  -> epoch 1736807220
// 2025-01-29 12:36 UTC = New  Moon  -> epoch 1738154160
int main(void) {
    moon_state_t full, neu;
    moon_compute((time_t)1736807220, 0.0, 0.0, &full);
    moon_compute((time_t)1738154160, 0.0, 0.0, &neu);

    printf("full illum=%.3f cycle=%.3f waxing=%d\n", full.illum, full.cycle, full.waxing);
    printf("new  illum=%.3f cycle=%.3f waxing=%d\n", neu.illum, neu.cycle, neu.waxing);

    int ok = 1;
    if (full.illum < 0.95f) { printf("FAIL: full moon illum too low\n"); ok = 0; }
    if (neu.illum  > 0.05f) { printf("FAIL: new moon illum too high\n");  ok = 0; }
    if (ok) { printf("PASS\n"); return 0; }
    return 1;
}
```

- [ ] **Step 3: Run it to confirm it fails before the source exists / passes after copy**

Run: `cc -O2 -o test/test_moon_eph test/test_moon_ephemeris.c moon_ephemeris.c -lm && ./test/test_moon_eph`
Expected after Step 1 copy: prints `full illum` near 1.0, `new illum` near 0.0, then `PASS`.
(If `cc` is unavailable on this host, use `gcc`. If neither exists, defer this test to the on-device serial output added in Task 8 and note the deferral in the PR.)

- [ ] **Step 4: Commit**

```bash
git add moon_ephemeris.h moon_ephemeris.c test/test_moon_ephemeris.c
git commit -m "feat: port lunar ephemeris with host unit test"
```

### Task 6: Port the moon renderer (flat-shaded, Arduino)

This ports `moon_render.c` minus the ESP-IDF-only logging/PNG-texture dependencies. It renders a smooth lit disc; surface texture is added later in Task 10. The pure rendering math (disc, terminator, edge AA, starfield, glow) is unchanged.

**Files:**
- Create: `moon_render.h`
- Create: `moon_render.cpp`

- [ ] **Step 1: Create `moon_render.h`**

```cpp
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
```

- [ ] **Step 2: Create `moon_render.cpp` (ported, flat texture fallback)**

```cpp
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
```

- [ ] **Step 3: Build to verify it compiles into the sketch**

Run: `pwsh -File .\compile-and-upload.ps1 -SkipUpload`
Expected: no error referencing `moon_render` or `moonRender`.

- [ ] **Step 4: Commit**

```bash
git add moon_render.h moon_render.cpp
git commit -m "feat: port moon disc renderer (flat-shaded)"
```

### Task 7: Add Moon configuration fields to ConfigStorage

**Files:**
- Modify: `config_storage.h` (add fields, dirty group, setters/getters)
- Modify: `config_storage.cpp` (defaults, load, save, accessors)
- Modify: `config.h` (defaults + new dirty constant if not adding to existing group)

- [ ] **Step 1: Add a dirty-group constant in `config_storage.h`**

After `DIRTY_LOGGING` (`config_storage.h:23`), add:

```cpp
static const uint32_t DIRTY_MOON        = 0x00001000;  // Moon lat, lon, background style
```

- [ ] **Step 2: Add config fields to the `Config` struct in `config_storage.h`**

After the `colorTemp` field (`:307`), add:

```cpp
        // Moon render settings
        float moonLat;        // observer latitude, degrees (0 => unset/north-up)
        float moonLon;        // observer longitude, degrees
        int   moonBgStyle;    // 0=black,1=stars,2=glow,3=stars+glow
```

- [ ] **Step 3: Declare setters/getters in `config_storage.h`**

Near the color-temp accessors (`:230-232`), add:

```cpp
    void setMoonLat(float lat);
    void setMoonLon(float lon);
    void setMoonBgStyle(int style);
    float getMoonLat();
    float getMoonLon();
    int  getMoonBgStyle();
```

- [ ] **Step 4: Add defaults in `config.h`**

After the color-temp defaults (`config.h:103-105`), add:

```cpp
// Moon render defaults
#define DEFAULT_MOON_LAT 0.0f       // 0 => north-up convention
#define DEFAULT_MOON_LON 0.0f
#define DEFAULT_MOON_BG_STYLE 1     // starfield
```

- [ ] **Step 5: Implement defaults/load/save/accessors in `config_storage.cpp`**

In `setDefaults()` add:

```cpp
    config.moonLat = DEFAULT_MOON_LAT;
    config.moonLon = DEFAULT_MOON_LON;
    config.moonBgStyle = DEFAULT_MOON_BG_STYLE;
```

In `loadConfig()` (match the existing `preferences.getX` style used for `colorTemp`):

```cpp
    config.moonLat = preferences.getFloat("moonLat", DEFAULT_MOON_LAT);
    config.moonLon = preferences.getFloat("moonLon", DEFAULT_MOON_LON);
    config.moonBgStyle = preferences.getInt("moonBg", DEFAULT_MOON_BG_STYLE);
```

In `saveConfig()`, inside the block guarded by the dirty check (add a `DIRTY_MOON` branch mirroring how `DIRTY_DISPLAY` is handled):

```cpp
    if (_dirtyFields & DIRTY_MOON) {
        preferences.putFloat("moonLat", config.moonLat);
        preferences.putFloat("moonLon", config.moonLon);
        preferences.putInt("moonBg", config.moonBgStyle);
    }
```

Add the accessors (mirror `setColorTemp`/`getColorTemp`):

```cpp
void ConfigStorage::setMoonLat(float lat)   { ConfigLock l(_mutex); config.moonLat = lat; markDirty(DIRTY_MOON); }
void ConfigStorage::setMoonLon(float lon)   { ConfigLock l(_mutex); config.moonLon = lon; markDirty(DIRTY_MOON); }
void ConfigStorage::setMoonBgStyle(int s)   { ConfigLock l(_mutex); config.moonBgStyle = s; markDirty(DIRTY_MOON); }
float ConfigStorage::getMoonLat()           { ConfigLock l(_mutex); return config.moonLat; }
float ConfigStorage::getMoonLon()           { ConfigLock l(_mutex); return config.moonLon; }
int  ConfigStorage::getMoonBgStyle()        { ConfigLock l(_mutex); return config.moonBgStyle; }
```

(Confirm the exact NVS key length limit: Preferences keys must be <= 15 chars. `moonLat`/`moonLon`/`moonBg` are fine.)

- [ ] **Step 6: Build to verify**

Run: `pwsh -File .\compile-and-upload.ps1 -SkipUpload`
Expected: no error.

- [ ] **Step 7: Commit**

```bash
git add config_storage.h config_storage.cpp config.h
git commit -m "feat: add moon lat/lon/background config to ConfigStorage"
```

### Task 8: Hook the moon sentinel URL into the download path

**Files:**
- Modify: `ESP32-P4-Allsky-Display.ino` (forward declaration near `:142`; branch in `downloadAndDisplayImage()` at `:1092`; new function near `renderFullImage()`)

- [ ] **Step 1: Add includes and a forward declaration**

Near the other includes at the top of the `.ino`:

```cpp
#include "moon_ephemeris.h"
#include "moon_render.h"
#include <time.h>
```

Near the existing forward declarations (`:142-148`):

```cpp
void renderMoonToPendingBuffer();
```

- [ ] **Step 2: Branch in `downloadAndDisplayImage()` before the HTTP path**

Immediately after `String imageURL = getCurrentImageURL();` (`:1092`), insert:

```cpp
    if (imageURL.startsWith("moon://")) {
        Serial.println("[Moon] Rendering computed moon image");
        renderMoonToPendingBuffer();
        systemMonitor.forceResetWatchdog();
        imageProcessing = false;   // matches the early-return contract used below
        return;
    }
```

- [ ] **Step 3: Implement `renderMoonToPendingBuffer()`**

Add near `renderFullImage()` (after it, around `:1059`). It renders at a fixed 600x600 (720KB, well within the 4MB `pendingFullImageBuffer`), copies into the pre-allocated pending buffer under `imageBufferMutex`, and signals the existing swap/render path. The moon source's own per-image transform (loaded by `updateCurrentImageTransformSettings()`) scales 600 to the panel.

```cpp
void renderMoonToPendingBuffer() {
    const int MOON_SIZE = 600;

    // Require a real wall-clock time (NTP). epoch < 2020-01-01 means unsynced.
    time_t now = time(nullptr);
    if (now < 1577836800) {
        Serial.println("[Moon] Clock not synced yet; skipping moon render this cycle");
        return;
    }

    moon_state_t st;
    moon_compute(now, (double)configStorage.getMoonLat(),
                      (double)configStorage.getMoonLon(), &st);
    Serial.printf("[Moon] %s illum=%.2f waxing=%d orient=%.2f\n",
                  st.phase_name, st.illum, st.waxing, st.orient_rad);

    uint8_t bg = (uint8_t)configStorage.getMoonBgStyle();
    uint16_t* moon = moonRender(MOON_SIZE, MOON_SIZE, &st, bg);
    if (!moon) { Serial.println("[Moon] render failed"); return; }

    size_t bytes = (size_t)MOON_SIZE * MOON_SIZE * 2;
    if (xSemaphoreTake(imageBufferMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
        if (pendingFullImageBuffer && bytes <= fullImageBufferSize) {
            memcpy(pendingFullImageBuffer, moon, bytes);
            pendingImageWidth  = MOON_SIZE;
            pendingImageHeight = MOON_SIZE;
            imageReadyToDisplay = true;
            Serial.println("[Moon] pending buffer filled, ready to display");
        } else {
            Serial.println("[Moon] pending buffer unavailable or too small");
        }
        xSemaphoreGive(imageBufferMutex);
    } else {
        Serial.println("[Moon] could not acquire image buffer mutex");
    }
    heap_caps_free(moon);
}
```

(Confirm the exact global names `imageBufferMutex`, `fullImageBufferSize`, `pendingImageHeight` are in scope here; all are file-level globals in the same `.ino` and used identically by the JPEG decode path at `:1499-1542`.)

- [ ] **Step 4: Seed a default moon source and its fit transform on first run (optional convenience)**

So the feature is reachable without manual URL entry, add a "moon://default" handling path in the web UI (Task 9). No `.ino` change needed here beyond the branch.

- [ ] **Step 5: Build, flash, and verify on device**

Run: `pwsh -File .\compile-and-upload.ps1`
Then via the web UI add a source with URL `moon://default` and set its transform scale to ~`displaySize/600` (e.g. 1.33 for the 800px panel).
Expected serial: `[Moon] <phase> illum=.. waxing=..` then `pending buffer filled` then `Buffer swap complete`. A centered lit moon disc appears, phase matching tonight's sky.

- [ ] **Step 6: Commit**

```bash
git add ESP32-P4-Allsky-Display.ino
git commit -m "feat: render computed moon via moon:// sentinel source"
```

### Task 9: Web UI for the Moon source (add button + lat/lon/background)

**Files:**
- Modify: `web_config_html.h` (moon controls)
- Modify: `web_config_api.cpp` (handler for moon settings; reuse `handleAddPreset` pattern or add `handleSetMoon`)
- Modify: `web_config.h` / `web_config.cpp` (declare + route)

- [ ] **Step 1: Add a "Moon" entry to the preset dropdown and a settings block**

In `web_config_html.h`, add to the preset `<select>` from Task 3:

```html
    <option value="__moon__">Moon (computed)</option>
```

Add a settings block near the image-sources section:

```html
<div class="moon-settings">
  <h4>Moon settings</h4>
  <label>Latitude <input type="number" id="moonLat" step="0.0001"></label>
  <label>Longitude <input type="number" id="moonLon" step="0.0001"></label>
  <label>Background
    <select id="moonBg">
      <option value="0">Black</option>
      <option value="1">Starfield</option>
      <option value="2">Glow</option>
      <option value="3">Stars + Glow</option>
    </select>
  </label>
  <button type="button" onclick="saveMoon()">Save Moon Settings</button>
</div>
```

- [ ] **Step 2: Extend `addPreset()` to handle the moon entry, and add `saveMoon()`**

In the page `<script>`:

```javascript
// In addPreset(): if the moon entry is selected, add the sentinel source instead.
function addPreset() {
  const id = document.getElementById('presetSelect').value;
  const url = (id === '__moon__') ? null : null; // handled server-side
  const body = (id === '__moon__') ? 'id=__moon__' : 'id=' + encodeURIComponent(id);
  fetch('/api/addPreset', { method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'}, body })
    .then(r => r.json()).then(j => { alert(j.message); location.reload(); })
    .catch(e => alert('Add failed: ' + e));
}

function saveMoon() {
  const p = new URLSearchParams();
  p.set('lat', document.getElementById('moonLat').value || '0');
  p.set('lon', document.getElementById('moonLon').value || '0');
  p.set('bg',  document.getElementById('moonBg').value || '1');
  fetch('/api/setMoon', { method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:p.toString() })
    .then(r => r.json()).then(j => alert(j.message))
    .catch(e => alert('Save failed: ' + e));
}
```

- [ ] **Step 3: Handle `__moon__` inside `handleAddPreset()` and add `handleSetMoon()`**

In `web_config_api.cpp`, at the top of `handleAddPreset()` (before the `findImagePreset` lookup), add:

```cpp
    if (server->arg("id") == "__moon__") {
        int newIndex = configStorage.getImageSourceCount();
        if (newIndex >= MAX_IMAGE_SOURCES) {
            sendResponse(400, "application/json", "{\"status\":\"error\",\"message\":\"Maximum image sources reached\"}");
            return;
        }
        configStorage.addImageSource("moon://default");
        int displaySize = displayManager.getWidth();
        float fit = (float)displaySize / 600.0f;     // moon renders at 600px
        if (fit < MIN_SCALE) fit = MIN_SCALE;
        if (fit > MAX_SCALE) fit = MAX_SCALE;
        configStorage.setImageScaleX(newIndex, fit);
        configStorage.setImageScaleY(newIndex, fit);
        configStorage.setImageOffsetX(newIndex, 0);
        configStorage.setImageOffsetY(newIndex, 0);
        configStorage.setImageRotation(newIndex, 0.0f);
        configStorage.saveConfig();
        sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Moon source added\"}");
        return;
    }
```

Add the moon-settings handler:

```cpp
void WebConfig::handleSetMoon() {
    if (server->hasArg("lat")) configStorage.setMoonLat(server->arg("lat").toFloat());
    if (server->hasArg("lon")) configStorage.setMoonLon(server->arg("lon").toFloat());
    if (server->hasArg("bg"))  configStorage.setMoonBgStyle(server->arg("bg").toInt());
    configStorage.saveConfig();
    sendResponse(200, "application/json", "{\"status\":\"success\",\"message\":\"Moon settings saved\"}");
}
```

Declare `void handleSetMoon();` in `web_config.h` and register `server->on("/api/setMoon", HTTP_POST, [this]() { handleSetMoon(); });` in `web_config.cpp` (matching the existing route style).

- [ ] **Step 4: Build, flash, verify**

Run: `pwsh -File .\compile-and-upload.ps1`
In the web UI: set lat/lon to your location, choose a background, Save; then add the "Moon (computed)" preset. Confirm the moon appears centered with the correct phase, and that the bright limb tilts correctly for your latitude (non-zero lat/lon enables the parallactic orientation).

- [ ] **Step 5: Commit**

```bash
git add web_config_html.h web_config_api.cpp web_config.h web_config.cpp
git commit -m "feat: web UI to add Moon source and configure location/background"
```

### Task 10: Bake the moon surface texture (replaces flat shading)

Optional quality pass. Replaces the flat `sampleTex()` with a real lunar surface (maria) sampled from a baked array, avoiding any PNG decoder.

**Files:**
- Create: `moon_texture_data.h` (generated)
- Create: `tools/bake_moon_texture.py` (one-shot generator)
- Modify: `moon_render.cpp` (use the baked texture in `sampleTex`)

- [ ] **Step 1: Write the texture baking script**

```python
# tools/bake_moon_texture.py
# Downsamples ESP32-P4-NINA-Display/main/moon_texture.png (500x500 RGBA)
# to 256x256 luminance+alpha and emits a C header (128 KB flash).
from PIL import Image
import sys

SRC = sys.argv[1] if len(sys.argv) > 1 else \
    r"C:\Users\Kumar\git\ESP32-P4-NINA-Display\main\moon_texture.png"
SIZE = 256
img = Image.open(SRC).convert("RGBA").resize((SIZE, SIZE), Image.LANCZOS)
px = img.load()
out = ["#pragma once", "#include <stdint.h>",
       f"#define MOON_TEX_SIZE {SIZE}",
       "// luminance,alpha pairs, row-major, MOON_TEX_SIZE^2 entries",
       "static const uint8_t MOON_TEX_LA[MOON_TEX_SIZE*MOON_TEX_SIZE*2] = {"]
vals = []
for y in range(SIZE):
    for x in range(SIZE):
        r, g, b, a = px[x, y]
        lum = (r*30 + g*59 + b*11) // 100
        vals.append(str(lum)); vals.append(str(a))
out.append(",".join(vals))
out.append("};")
open("moon_texture_data.h", "w").write("\n".join(out))
print("wrote moon_texture_data.h", SIZE, "x", SIZE)
```

- [ ] **Step 2: Generate the header**

Run: `python tools/bake_moon_texture.py`
Expected: `wrote moon_texture_data.h 256 x 256` and a `moon_texture_data.h` (~1.2 MB source, ~128 KB flash) in the project root.

- [ ] **Step 3: Use the baked texture in `moon_render.cpp`**

Add `#include "moon_texture_data.h"` near the top, then replace `sampleTex()` with:

```cpp
static inline void sampleTex(float nx, float ny, int* lum, int* alpha) {
    int tx = (int)((nx * 0.5f + 0.5f) * (MOON_TEX_SIZE - 1) + 0.5f);
    int ty = (int)((ny * 0.5f + 0.5f) * (MOON_TEX_SIZE - 1) + 0.5f);
    if (tx < 0 || tx >= MOON_TEX_SIZE || ty < 0 || ty >= MOON_TEX_SIZE) { *lum = 0; *alpha = 0; return; }
    const uint8_t* p = &MOON_TEX_LA[((size_t)ty * MOON_TEX_SIZE + tx) * 2];
    *lum = p[0]; *alpha = p[1];
}
```

- [ ] **Step 4: Build, flash, verify**

Run: `pwsh -File .\compile-and-upload.ps1`
Expected: the moon now shows surface detail (maria). Confirm flash usage still fits the 13MB app partition (watch the build's flash-usage summary).

- [ ] **Step 5: Commit**

```bash
git add moon_texture_data.h tools/bake_moon_texture.py moon_render.cpp
git commit -m "feat: bake lunar surface texture into moon renderer"
```

---

## Self-Review

**Spec coverage:**
- "Include solar features" -> Tasks 1-4 (preset table, web picker, per-source fit transform, HTTPS verification).
- "Include lunar features" -> Tasks 5-10 (ephemeris, renderer, config, pipeline hook, web UI, texture).
- "Scale and center on circular display" -> centering is already correct (`renderFullImage` `:864-869`); fit scaling seeded per source in Tasks 2 and 9; moon renderer fills 0.92 of the disc.
- "Each solar image source has its own scale/transform settings" -> guaranteed by the existing `imageTransforms[10]` array; explicitly seeded per new source in `handleAddPreset()` (Task 2) and the moon add path (Task 9).

**Open verifications to perform during execution (not assumptions):**
- HTTPS handshake to `sdo.gsfc.nasa.gov` / `soho.nascom.nasa.gov` / `cdn.star.nesdis.noaa.gov` (Task 4). Fallback coded.
- Exact route-registration syntax in `web_config.cpp` (copy the existing `/api/addImageSource` line's form).
- Exact `loadConfig`/`saveConfig` Preferences style in `config_storage.cpp` (mirror `colorTemp`).
- Host `cc`/`gcc` availability for the ephemeris test (Task 5); else defer to on-device serial.
- Flash budget after the 128 KB texture (Task 10).

**Type consistency:** `moon_state_t` fields (`illum`, `waxing`, `orient_rad`, `phase_name`) are used identically across ephemeris, renderer, and the `.ino` hook. The renderer entry point is `moonRender()` everywhere (renamed from NINA's `moon_render()` to avoid colliding with the `moon_render` translation-unit name). Globals `pendingFullImageBuffer`, `pendingImageWidth`, `pendingImageHeight`, `imageReadyToDisplay`, `imageBufferMutex`, `fullImageBufferSize` match the JPEG decode path.

**Sequencing note:** Phase 1 ships independently and is low-risk. Phase 2 is independent of Phase 1; Task 10 is optional polish and can be deferred without blocking a working moon.
