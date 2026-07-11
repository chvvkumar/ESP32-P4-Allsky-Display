/**
 * @file moon_sphere.cpp
 * @brief Textured, sub-solar-lit 3D Moon sphere rendered with the tgx software
 *        renderer (https://github.com/vindar/tgx).
 *
 * ESP32-P4 notes:
 *  - The P4 FPU is single precision only. tgx defaults TGX_SINGLE_PRECISION_COMPUTATIONS
 *    to 1, and all math in this file uses float / sqrtf. No double in hot paths.
 *  - Big buffers (color + z-buffer) live in PSRAM, 128-byte aligned for PPA.
 *  - The lunar albedo texture is embedded as a raw little-endian RGB565 blob
 *    (1024x512, power-of-two for SHADER_TEXTURE_WRAP_POW2) so no runtime image
 *    decoder is required.
 */

#include "sdkconfig.h"

/* Force single precision before pulling in tgx (matches tgx default; explicit
 * here in case a global -D ever flips it). */
#ifndef TGX_SINGLE_PRECISION_COMPUTATIONS
#define TGX_SINGLE_PRECISION_COMPUTATIONS 1
#endif

#include "moon_sphere.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <string.h>

#include <tgx.h>

/* Runtime orientation config (moon flip U/V, roll/yaw/pitch trims, north-up).
 * app_config.h self-guards with extern "C", so include it at file scope. */
#include "app_config.h"

using namespace tgx;

static const char *TAG = "moon_sphere";

/* Live moon orientation config, snapshot once per render through the config-spine
 * typed getters (each takes the config mutex internally). */
struct moon_cfg_t {
    int   flip_u;
    int   flip_v;
    int   north_up;
    float roll_off;
    float yaw_off;
    float pitch_off;
};

static void read_moon_cfg(moon_cfg_t *c)
{
    c->flip_u    = app_config_get_mFlipU() ? 1 : 0;
    c->flip_v    = app_config_get_mFlipV() ? 1 : 0;
    c->north_up  = app_config_get_mNorthUp() ? 1 : 0;
    c->roll_off  = app_config_get_mRollOff();
    c->yaw_off   = app_config_get_mYawOff();
    c->pitch_off = app_config_get_mPitchOff();
}

/* Compile-time debug: force roll=0 AND libration=0 to expose the north-up base
 * orientation for comparison against a reference photo. Default off. */
#ifndef MOON_ROLL_SIGN
#define MOON_ROLL_SIGN      (+1) /* flip to -1 if the disc rolls the wrong way */
#endif
#ifndef MOON_DEBUG_GEOCENTRIC
#define MOON_DEBUG_GEOCENTRIC 0
#endif

/* RGB565 packer with clamping, named pack565() so it does not collide with the
 * tgx::RGB565 type. Used only to draw the starfield + glow background directly
 * into the color buffer. */
static inline uint16_t pack565(int r, int g, int b)
{
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* 4x4 ordered-dither (Bayer) matrix used to fill RGB565 truncation slack in the
 * warm glow halo so it does not band. */
static const uint8_t s_bayer4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5}
};

/* Embedded lunar albedo texture: raw little-endian RGB565, 1024x512 (power of
 * two). Public-domain USGS Clementine 750nm equirectangular map, north-up,
 * longitude -180..+180 left-to-right with 0 centered. Embedded via
 * CMakeLists.txt EMBED_FILES. */
extern const uint8_t moon_equirect_rgb565_start[] asm("_binary_moon_equirect_rgb565_start");
extern const uint8_t moon_equirect_rgb565_end[]   asm("_binary_moon_equirect_rgb565_end");

static const int EMBED_TEX_W = 1024;
static const int EMBED_TEX_H = 512;

/* Procedural placeholder dimensions (power-of-two) used only if the PSRAM copy
 * of the embedded texture cannot be allocated. */
static const int PLACEHOLDER_TEX_W = 512;
static const int PLACEHOLDER_TEX_H = 256;

static uint16_t        *s_tex_buf = nullptr;   /* RGB565, s_tex_w*s_tex_h, PSRAM */
static int              s_tex_w   = 0;
static int              s_tex_h   = 0;
static Image<RGB565>    s_tex;
static bool             s_inited = false;
static SemaphoreHandle_t s_init_mtx = nullptr;

/* Flip state actually baked into s_tex_buf, for live re-flip detection.
 * -1 = "no texture prepared yet". */
static int              s_applied_flip_u = -1;
static int              s_applied_flip_v = -1;

static void generate_placeholder_texture()
{
    struct { float u, v, r, depth; } maria[] = {
        { 0.32f, 0.42f, 0.13f, 0.45f },
        { 0.44f, 0.36f, 0.09f, 0.40f },
        { 0.55f, 0.30f, 0.07f, 0.35f },
        { 0.40f, 0.55f, 0.10f, 0.30f },
        { 0.62f, 0.50f, 0.06f, 0.25f },
        { 0.26f, 0.30f, 0.05f, 0.20f },
    };
    const int n_maria = (int)(sizeof(maria) / sizeof(maria[0]));

    for (int y = 0; y < PLACEHOLDER_TEX_H; y++) {
        float v = (float)y / (float)(PLACEHOLDER_TEX_H - 1);
        for (int x = 0; x < PLACEHOLDER_TEX_W; x++) {
            float u = (float)x / (float)(PLACEHOLDER_TEX_W - 1);

            float base = 0.72f
                       + 0.06f * sinf(u * 18.0f)
                       + 0.05f * sinf(v * 22.0f + 1.3f)
                       + 0.03f * sinf((u + v) * 40.0f);

            float dark = 0.0f;
            for (int m = 0; m < n_maria; m++) {
                float du = u - maria[m].u;
                if (du >  0.5f) du -= 1.0f;
                if (du < -0.5f) du += 1.0f;
                float dv = v - maria[m].v;
                float d2 = du * du + dv * dv;
                float r2 = maria[m].r * maria[m].r;
                if (d2 < r2) {
                    float t = 1.0f - sqrtf(d2) / maria[m].r;
                    dark += maria[m].depth * t * t;
                }
            }

            float g = base - dark;
            if (g < 0.05f) g = 0.05f;
            if (g > 1.00f) g = 1.00f;
            s_tex(x, y) = RGB565(g, g, g);
        }
    }
}

/* Allocate the PSRAM RGB565 texture buffer for w*h pixels and wrap it with
 * s_tex. Returns true on success and sets s_tex_w/s_tex_h. */
static bool alloc_texture(int w, int h)
{
    if (s_tex_buf) {
        heap_caps_free(s_tex_buf);
        s_tex_buf = nullptr;
    }
    size_t bytes = (size_t)w * (size_t)h * sizeof(uint16_t);
    s_tex_buf = (uint16_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!s_tex_buf) {
        ESP_LOGE(TAG, "PSRAM lunar texture alloc failed (%dx%d, %u bytes)",
                 w, h, (unsigned)bytes);
        return false;
    }
    s_tex_w = w;
    s_tex_h = h;
    s_tex.set(s_tex_buf, w, h, w);
    return true;
}

static void mirror_texture_u(void)
{
    for (int y = 0; y < s_tex_h; y++) {
        uint16_t *row = s_tex_buf + (size_t)y * (size_t)s_tex_w;
        for (int x = 0; x < s_tex_w / 2; x++) {
            uint16_t t = row[x];
            row[x] = row[s_tex_w - 1 - x];
            row[s_tex_w - 1 - x] = t;
        }
    }
}

static void mirror_texture_v(void)
{
    for (int y = 0; y < s_tex_h / 2; y++) {
        uint16_t *a = s_tex_buf + (size_t)y * (size_t)s_tex_w;
        uint16_t *b = s_tex_buf + (size_t)(s_tex_h - 1 - y) * (size_t)s_tex_w;
        for (int x = 0; x < s_tex_w; x++) {
            uint16_t t = a[x];
            a[x] = b[x];
            b[x] = t;
        }
    }
}

static void apply_texture_flips(void)
{
    moon_cfg_t cfg;
    read_moon_cfg(&cfg);

    if (cfg.flip_u) mirror_texture_u();
    if (cfg.flip_v) mirror_texture_v();

    s_applied_flip_u = cfg.flip_u;
    s_applied_flip_v = cfg.flip_v;
}

static bool init_placeholder_texture(void)
{
    if (!alloc_texture(PLACEHOLDER_TEX_W, PLACEHOLDER_TEX_H)) return false;
    generate_placeholder_texture();
    apply_texture_flips();
    ESP_LOGW(TAG, "using procedural placeholder lunar texture %dx%d "
                  "(embedded texture unavailable)",
             PLACEHOLDER_TEX_W, PLACEHOLDER_TEX_H);
    return true;
}

/* Copy the embedded raw RGB565 lunar map into the PSRAM texture buffer. Returns
 * true on success. On any failure leaves s_tex_buf NULL so the caller can fall
 * back to the placeholder. */
static bool init_embedded_texture(void)
{
    size_t len = (size_t)(moon_equirect_rgb565_end - moon_equirect_rgb565_start);
    size_t need = (size_t)EMBED_TEX_W * (size_t)EMBED_TEX_H * sizeof(uint16_t);
    if (len < need) {
        ESP_LOGW(TAG, "embedded moon texture too small (%u < %u)",
                 (unsigned)len, (unsigned)need);
        return false;
    }

    if (!alloc_texture(EMBED_TEX_W, EMBED_TEX_H)) {
        return false;
    }

    /* Little-endian RGB565 in flash matches the P4's native uint16 layout. */
    memcpy(s_tex_buf, moon_equirect_rgb565_start, need);

    apply_texture_flips();
    ESP_LOGI(TAG, "loaded embedded lunar texture %dx%d (RGB565)",
             EMBED_TEX_W, EMBED_TEX_H);
    return true;
}

extern "C" bool moon_sphere_init(void)
{
    if (!s_init_mtx) s_init_mtx = xSemaphoreCreateMutex();
    if (s_init_mtx) xSemaphoreTake(s_init_mtx, portMAX_DELAY);

    if (!s_inited) {
        if (init_embedded_texture() || init_placeholder_texture()) {
            s_inited = true;
        }
    }

    if (s_init_mtx) xSemaphoreGive(s_init_mtx);
    return s_inited;
}

/* Apply a live flip-config change to the EXISTING decoded texture buffer in
 * place. A flip is a row/column mirror, its own inverse, so toggling an axis =
 * mirroring that axis once. Guarded by the init mutex. No-op when the flip state
 * already matches. */
static void moon_sphere_reflip_if_changed(void)
{
    moon_cfg_t cfg;
    read_moon_cfg(&cfg);
    if (cfg.flip_u == s_applied_flip_u && cfg.flip_v == s_applied_flip_v) return;

    if (s_init_mtx) xSemaphoreTake(s_init_mtx, portMAX_DELAY);
    if (cfg.flip_u != s_applied_flip_u || cfg.flip_v != s_applied_flip_v) {
        if (s_tex_buf == nullptr) {
            if (!init_embedded_texture()) init_placeholder_texture();
        } else {
            if (cfg.flip_u != s_applied_flip_u) {
                mirror_texture_u();
                s_applied_flip_u = cfg.flip_u;
            }
            if (cfg.flip_v != s_applied_flip_v) {
                mirror_texture_v();
                s_applied_flip_v = cfg.flip_v;
            }
        }
    }
    if (s_init_mtx) xSemaphoreGive(s_init_mtx);
}

static const Shader LOADED_SHADERS =
    SHADER_ORTHO | SHADER_ZBUFFER | SHADER_GOURAUD |
    SHADER_TEXTURE_BILINEAR | SHADER_TEXTURE_WRAP_POW2;

/* Core sphere renderer. Rasterizes into `color_buf` (RGB565) using `zbuf` as the
 * depth buffer; both must be w*h uint16 and 128-byte aligned. The caller owns
 * both buffers and their lifetime. */
static uint16_t *moon_sphere_render_core(int w, int h, const moon_state_t *st,
                                         int nb_sectors, int nb_stacks,
                                         uint8_t bg_style,
                                         float yaw_deg, float pitch_deg,
                                         moon_light_mode_t light_mode,
                                         float disk_scale,
                                         uint16_t *color_buf, uint16_t *zbuf)
{
    const size_t npix      = (size_t)w * (size_t)h;
    const size_t color_sz  = npix * sizeof(uint16_t);

    /* Disc sizing: disk_scale is the fraction of the frame the disc fills. The
     * orthographic half-extent is 1.08 / disk_scale, so 1.0 fills the frame and
     * smaller values shrink the disc (background fills the rest). Clamp defends
     * the projection math against out-of-range per-source values. */
    if (disk_scale < 0.2f) disk_scale = 0.2f;
    if (disk_scale > 4.0f) disk_scale = 4.0f;
    const float ORTHO_R = 1.08f / disk_scale;

    /* Live orientation config, read once. */
    moon_cfg_t cfg;
    read_moon_cfg(&cfg);

    /* If the flip config changed since the texture was last prepared, re-apply
     * now so web-UI flip toggles take effect live. No-op when unchanged. */
    moon_sphere_reflip_if_changed();

    /* ----- Background (drawn into color_buf BEFORE the sphere) ------------ */
    Image<RGB565> im(color_buf, w, h, w);

    memset(color_buf, 0, color_sz);   /* black base */

    if (bg_style & 1) {
        uint32_t seed = 1234567u;
        int nstars = (w * h) / 900;
        for (int i = 0; i < nstars; i++) {
            seed = seed * 1103515245u + 12345u;
            int sx = (int)((seed >> 8) % (uint32_t)w);
            seed = seed * 1103515245u + 12345u;
            int sy = (int)((seed >> 8) % (uint32_t)h);
            int br = 120 + (int)((seed >> 4) & 0x7F);
            color_buf[(size_t)sy * w + sx] = pack565(br, br, br);
        }
    }

    if (bg_style & 2) {
        const float R_disc  = (1.0f / ORTHO_R) * 0.5f * (float)((w < h) ? w : h);
        const float cx = (float)(w - 1) * 0.5f;
        const float cy = (float)(h - 1) * 0.5f;
        const float R_disc2 = R_disc * R_disc;
        const float Rout    = R_disc * 1.35f;
        const float Rout2   = Rout * Rout;
        const float inv_band = 1.0f / (R_disc * 0.35f);
        for (int py = 0; py < h; py++) {
            float dy = (float)py - cy;
            uint16_t *row = color_buf + (size_t)py * w;
            for (int px = 0; px < w; px++) {
                float dx = (float)px - cx;
                float r2 = dx * dx + dy * dy;
                if (r2 <= R_disc2 || r2 >= Rout2) continue;
                float dist = sqrtf(r2);
                float t = 1.0f - (dist - R_disc) * inv_band;
                int add = (int)(40.0f * t);
                uint16_t c = row[px];
                int r = ((c >> 11) & 0x1F) * 255 / 31 + add;
                int g = ((c >> 5)  & 0x3F) * 255 / 63 + add * 9 / 10;
                int b = ( c        & 0x1F) * 255 / 31 + add * 7 / 10;
                int dr = s_bayer4[py & 3][px & 3] >> 1;
                int dg = s_bayer4[py & 3][px & 3] >> 2;
                int db = dr;
                row[px] = pack565(r + dr, g + dg, b + db);
            }
        }
    }

    /* ----- Set up the renderer ------------------------------------------- */
    Renderer3D<RGB565, LOADED_SHADERS, uint16_t> renderer;
    renderer.setViewportSize(w, h);
    renderer.setOffset(0, 0);
    renderer.setImage(&im);
    renderer.setZbuffer(zbuf);
    renderer.clearZbuffer();
    renderer.setCulling(1);

    renderer.setOrtho(-ORTHO_R, ORTHO_R, -ORTHO_R, ORTHO_R, 0.1f, 10.0f);

    renderer.setShaders(SHADER_GOURAUD | SHADER_TEXTURE_BILINEAR | SHADER_TEXTURE_WRAP_POW2);
    renderer.setMaterialColor(RGBf(1.0f, 0.96f, 0.86f));
    renderer.setMaterialAmbiantStrength(0.06f);
    renderer.setMaterialDiffuseStrength(1.0f);
    renderer.setMaterialSpecularStrength(0.0f);

    renderer.setLightAmbiant(RGBf(1.0f, 1.0f, 1.0f));
    renderer.setLightDiffuse(RGBf(1.0f, 1.0f, 1.0f));
    renderer.setLightSpecular(RGBf(0.0f, 0.0f, 0.0f));

    /* ----- Model matrix: orient the disc --------------------------------- */
#if MOON_DEBUG_GEOCENTRIC
    float lib_lon_deg = 0.0f;
    float lib_lat_deg = 0.0f;
    float roll_deg    = 0.0f;
    yaw_deg   = 0.0f;
    pitch_deg = 0.0f;
#else
    const float RAD2DEG = 57.2957795131f;
    float lib_lon_deg = st->lib_lon * RAD2DEG;
    float lib_lat_deg = st->lib_lat * RAD2DEG;
    /* north-up: ignore the parallactic/topocentric tilt (st->roll) and keep the
     * disc north-up; libration, phase, and the live roll trim still apply. */
    float roll_deg    = (cfg.north_up ? 0.0f
                         : (float)(MOON_ROLL_SIGN) * st->roll * RAD2DEG)
                        + cfg.roll_off;
    yaw_deg   += cfg.yaw_off;
    pitch_deg += cfg.pitch_off;
#endif

    /* R_sky: the SKY orientation rotation (libration + base + parallactic roll),
     * used UNCHANGED to place the sub-solar light so the sun stays fixed in the
     * sky regardless of the user's drag yaw/pitch. */
    fMat4 R_sky;
    R_sky.setIdentity();
    R_sky.multRotate(lib_lon_deg,  fVec3(0.0f, 1.0f, 0.0f));
    R_sky.multRotate(-lib_lat_deg, fVec3(0.0f, 0.0f, 1.0f));
    R_sky.multRotate(180.0f,       fVec3(1.0f, 0.0f, 0.0f));
    R_sky.multRotate(90.0f,        fVec3(0.0f, 1.0f, 0.0f));
    R_sky.multRotate(roll_deg,     fVec3(0.0f, 0.0f, 1.0f));

    /* M_rot: sky orientation with the user drag applied OUTERMOST (pitch about
     * screen-horizontal +X, yaw about screen-vertical +Y). */
    fMat4 M_rot = R_sky;
    M_rot.multRotate(pitch_deg, fVec3(1.0f, 0.0f, 0.0f));
    M_rot.multRotate(yaw_deg,   fVec3(0.0f, 1.0f, 0.0f));

    fMat4 M = M_rot;
    M.multTranslate(fVec3(0.0f, 0.0f, -2.0f));
    renderer.setModelMatrix(M);

    /* ----- Lighting: directional light from the sub-solar point ---------- */
    float clat = cosf(st->sun_lat);
    fVec3 sun_body(clat * cosf(st->sun_lon),
                   sinf(st->sun_lat),
                   clat * sinf(st->sun_lon));
    fVec4 sun_w4 = R_sky.mult0(sun_body);
    fVec3 sun_w(sun_w4.x, sun_w4.y, sun_w4.z);
    float n = sqrtf(sun_w.x * sun_w.x + sun_w.y * sun_w.y + sun_w.z * sun_w.z);
    if (n > 1e-6f) { sun_w.x /= n; sun_w.y /= n; sun_w.z /= n; }

    if (light_mode == MOON_LIGHT_SURFACE_LOCKED) {
        fMat4 R_drag;
        R_drag.setIdentity();
        R_drag.multRotate(pitch_deg, fVec3(1.0f, 0.0f, 0.0f));
        R_drag.multRotate(yaw_deg,   fVec3(0.0f, 1.0f, 0.0f));
        fVec4 sun_d4 = R_drag.mult0(sun_w);
        sun_w.x = sun_d4.x; sun_w.y = sun_d4.y; sun_w.z = sun_d4.z;
        float nd = sqrtf(sun_w.x * sun_w.x + sun_w.y * sun_w.y + sun_w.z * sun_w.z);
        if (nd > 1e-6f) { sun_w.x /= nd; sun_w.y /= nd; sun_w.z /= nd; }
    }

    if (light_mode == MOON_LIGHT_EXPLORE) {
        renderer.setMaterialAmbiantStrength(0.95f);
        renderer.setMaterialDiffuseStrength(0.15f);
        renderer.setLightDirection(fVec3(0.0f, 0.0f, -1.0f));
    } else {
        renderer.setLightDirection(fVec3(sun_w.x, sun_w.y, sun_w.z));
    }

    renderer.drawSphere(nb_sectors, nb_stacks, &s_tex);

    return color_buf;
}

extern "C" uint16_t *moon_sphere_render_into(int w, int h, const moon_state_t *st,
                                             int nb_sectors, int nb_stacks,
                                             uint8_t bg_style,
                                             float yaw_deg, float pitch_deg,
                                             moon_light_mode_t light_mode,
                                             float disk_scale,
                                             uint16_t *color_buf, uint16_t *zbuf)
{
    if (w <= 0 || h <= 0 || st == nullptr || color_buf == nullptr || zbuf == nullptr)
        return nullptr;
    if (!moon_sphere_init()) return nullptr;
    return moon_sphere_render_core(w, h, st, nb_sectors, nb_stacks, bg_style,
                                   yaw_deg, pitch_deg, light_mode, disk_scale,
                                   color_buf, zbuf);
}

extern "C" uint16_t *moon_sphere_render_ex(int w, int h, const moon_state_t *st,
                                           int nb_sectors, int nb_stacks,
                                           uint8_t bg_style,
                                           float yaw_deg, float pitch_deg,
                                           moon_light_mode_t light_mode,
                                           float disk_scale)
{
    if (w <= 0 || h <= 0 || st == nullptr) return nullptr;
    if (!moon_sphere_init()) return nullptr;

    const size_t npix      = (size_t)w * (size_t)h;
    const size_t color_sz  = npix * sizeof(uint16_t);
    const size_t z_sz      = npix * sizeof(uint16_t);

    uint16_t *color_buf =
        (uint16_t *)heap_caps_aligned_alloc(128, color_sz, MALLOC_CAP_SPIRAM);
    uint16_t *zbuf =
        (uint16_t *)heap_caps_aligned_alloc(128, z_sz, MALLOC_CAP_SPIRAM);

    if (!color_buf || !zbuf) {
        ESP_LOGE(TAG, "PSRAM alloc failed (color=%p z=%p, %dx%d)",
                 (void *)color_buf, (void *)zbuf, w, h);
        if (color_buf) heap_caps_free(color_buf);
        if (zbuf)      heap_caps_free(zbuf);
        return nullptr;
    }

    moon_sphere_render_core(w, h, st, nb_sectors, nb_stacks,
                            bg_style, yaw_deg, pitch_deg,
                            light_mode, disk_scale, color_buf, zbuf);

    heap_caps_free(zbuf);
    return color_buf;
}

extern "C" uint16_t *moon_sphere_render(int w, int h, const moon_state_t *st,
                                        int nb_sectors, int nb_stacks,
                                        uint8_t bg_style, float disk_scale)
{
    return moon_sphere_render_ex(w, h, st, nb_sectors, nb_stacks, bg_style,
                                 0.0f, 0.0f, MOON_LIGHT_TRUE_PHASE, disk_scale);
}
