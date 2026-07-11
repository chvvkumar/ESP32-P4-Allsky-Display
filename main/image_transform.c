/**
 * @file image_transform.c
 * @brief Scale / rotate / pan / color-temperature compose stage.
 */

#include "image_transform.h"
#include "image_ppa.h"

#include <math.h>
#include <string.h>

#include "esp_cache.h"
#include "esp_log.h"

static const char *TAG = "img_xform";

static uint16_t *s_scaled;
static size_t    s_scaled_cap;
static int       s_pw, s_ph;

/* Reuse cache: last scaled (pre-rotation, pre-color-temp) buffer. */
static bool     s_cache_valid;
static uint32_t s_cache_gen;
static int      s_cache_bw, s_cache_bh;

/* Color-temperature per-channel multipliers in 8.8 fixed point (256 = 1.0). */
typedef struct { uint32_t rm, gm, bm; } ct_mult_t;

esp_err_t image_transform_init(uint16_t *scaled_buf, size_t scaled_cap,
                               int panel_w, int panel_h)
{
    if (!scaled_buf || panel_w <= 0 || panel_h <= 0) {
        return ESP_ERR_INVALID_ARG;
    }
    s_scaled = scaled_buf;
    s_scaled_cap = scaled_cap;
    s_pw = panel_w;
    s_ph = panel_h;
    s_cache_valid = false;
    return ESP_OK;
}

void image_transform_invalidate_cache(void)
{
    s_cache_valid = false;
}

/* Tanner-Helland blackbody approximation, normalized so 6500 K -> (1,1,1). */
static void channel_rgb(double kelvin, double *r, double *g, double *b)
{
    double t = kelvin / 100.0;
    double rr, gg, bb;
    if (t <= 66.0) {
        rr = 255.0;
    } else {
        rr = 329.698727446 * pow(t - 60.0, -0.1332047592);
    }
    if (t <= 66.0) {
        gg = 99.4708025861 * log(t) - 161.1195681661;
    } else {
        gg = 288.1221695283 * pow(t - 60.0, -0.0755148492);
    }
    if (t >= 66.0) {
        bb = 255.0;
    } else if (t <= 19.0) {
        bb = 0.0;
    } else {
        bb = 138.5177312231 * log(t - 10.0) - 305.0447927307;
    }
    if (rr < 1.0) rr = 1.0; if (rr > 255.0) rr = 255.0;
    if (gg < 1.0) gg = 1.0; if (gg > 255.0) gg = 255.0;
    if (bb < 1.0) bb = 1.0; if (bb > 255.0) bb = 255.0;
    *r = rr; *g = gg; *b = bb;
}

static ct_mult_t ct_mults(int kelvin)
{
    double tr, tg, tb, nr, ng, nb;
    channel_rgb((double)kelvin, &tr, &tg, &tb);
    channel_rgb(6500.0, &nr, &ng, &nb);
    ct_mult_t m;
    m.rm = (uint32_t)lround(256.0 * tr / nr);
    m.gm = (uint32_t)lround(256.0 * tg / ng);
    m.bm = (uint32_t)lround(256.0 * tb / nb);
    return m;
}

static inline uint16_t apply_ct(uint16_t px, const ct_mult_t *m)
{
    uint32_t r = (px >> 11) & 0x1F;
    uint32_t g = (px >> 5) & 0x3F;
    uint32_t b = px & 0x1F;
    r = (r * m->rm) >> 8; if (r > 31) r = 31;
    g = (g * m->gm) >> 8; if (g > 63) g = 63;
    b = (b * m->bm) >> 8; if (b > 31) b = 31;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* Software bilinear scale of src (iw x ih, stride) -> dst (bw x bh). */
static void sw_scale(const uint16_t *src, int iw, int ih, int stride,
                     uint16_t *dst, int bw, int bh, void (*wdt_feed)(void))
{
    /* 16.16 fixed step. */
    uint32_t sx_step = ((uint32_t)iw << 16) / (uint32_t)bw;
    uint32_t sy_step = ((uint32_t)ih << 16) / (uint32_t)bh;
    uint32_t syf = 0;
    for (int y = 0; y < bh; ++y) {
        int sy = syf >> 16;
        if (sy >= ih) sy = ih - 1;
        const uint16_t *row = src + (size_t)sy * stride;
        uint32_t sxf = 0;
        uint16_t *drow = dst + (size_t)y * bw;
        for (int x = 0; x < bw; ++x) {
            int sx = sxf >> 16;
            if (sx >= iw) sx = iw - 1;
            drow[x] = row[sx];
            sxf += sx_step;
        }
        syf += sy_step;
        if (wdt_feed && (y & 63) == 0) {
            wdt_feed();
        }
    }
}

/* Displayed scaled dimensions after a rotation (degrees 0/90/180/270). */
static void disp_dims(int rot, int bw, int bh, int *dw, int *dh)
{
    if (rot == 90 || rot == 270) {
        *dw = bh; *dh = bw;
    } else {
        *dw = bw; *dh = bh;
    }
}

/* Map a displayed coord (u,v) back to a scaled-buffer coord (bx,by) for the
 * rotation. Displayed dims are (dw,dh); buffer dims (bw,bh). */
static inline void rot_map(int rot, int u, int v, int bw, int bh, int *bx, int *by)
{
    switch (rot) {
        case 90:  *bx = v;          *by = bh - 1 - u; break;
        case 180: *bx = bw - 1 - u; *by = bh - 1 - v; break;
        case 270: *bx = bw - 1 - v; *by = u;          break;
        default:  *bx = u;          *by = v;          break;
    }
}

/* Compose a pre-scaled buffer (bw x bh, tightly packed) into present. */
static void compose_from_scaled(const uint16_t *scaled, int bw, int bh, int rot,
                                int offx, int offy, const ct_mult_t *ct, bool ct_on,
                                uint16_t *present, void (*wdt_feed)(void))
{
    int dw, dh;
    disp_dims(rot, bw, bh, &dw, &dh);
    int x0 = s_pw / 2 - dw / 2 + offx;
    int y0 = s_ph / 2 - dh / 2 + offy;

    memset(present, 0, (size_t)s_pw * s_ph * 2);

    int px_lo = x0 < 0 ? 0 : x0;
    int py_lo = y0 < 0 ? 0 : y0;
    int px_hi = (x0 + dw) > s_pw ? s_pw : (x0 + dw);
    int py_hi = (y0 + dh) > s_ph ? s_ph : (y0 + dh);

    for (int py = py_lo; py < py_hi; ++py) {
        int v = py - y0;
        uint16_t *prow = present + (size_t)py * s_pw;
        for (int px = px_lo; px < px_hi; ++px) {
            int u = px - x0;
            int bx, by;
            rot_map(rot, u, v, bw, bh, &bx, &by);
            uint16_t s = scaled[(size_t)by * bw + bx];
            prow[px] = ct_on ? apply_ct(s, ct) : s;
        }
        if (wdt_feed && (py & 63) == 0) {
            wdt_feed();
        }
    }
    esp_cache_msync(present, (size_t)s_pw * s_ph * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

/* Software inverse-map compose directly from the active buffer (no intermediate
 * scaled buffer): used when the fully scaled image would not fit the scratch
 * buffer, or as the PPA fallback. @p bw / @p bh are the scaled (pre-rotation)
 * dimensions the buffer would have had. Nearest-neighbour sampling. */
static void compose_inverse(const uint16_t *active, int iw, int ih, int stride,
                            int bw, int bh, int rot, int offx, int offy,
                            const ct_mult_t *ct, bool ct_on,
                            uint16_t *present, void (*wdt_feed)(void))
{
    if (bw < 1) bw = 1;
    if (bh < 1) bh = 1;
    int dw, dh;
    disp_dims(rot, bw, bh, &dw, &dh);
    int x0 = s_pw / 2 - dw / 2 + offx;
    int y0 = s_ph / 2 - dh / 2 + offy;

    memset(present, 0, (size_t)s_pw * s_ph * 2);

    int px_lo = x0 < 0 ? 0 : x0;
    int py_lo = y0 < 0 ? 0 : y0;
    int px_hi = (x0 + dw) > s_pw ? s_pw : (x0 + dw);
    int py_hi = (y0 + dh) > s_ph ? s_ph : (y0 + dh);

    for (int py = py_lo; py < py_hi; ++py) {
        int v = py - y0;
        uint16_t *prow = present + (size_t)py * s_pw;
        for (int px = px_lo; px < px_hi; ++px) {
            int u = px - x0;
            int bx, by; /* scaled-space coords */
            rot_map(rot, u, v, bw, bh, &bx, &by);
            int srcx = (int)((int64_t)bx * iw / bw);
            int srcy = (int)((int64_t)by * ih / bh);
            if (srcx < 0) srcx = 0; else if (srcx >= iw) srcx = iw - 1;
            if (srcy < 0) srcy = 0; else if (srcy >= ih) srcy = ih - 1;
            uint16_t s = active[(size_t)srcy * stride + srcx];
            prow[px] = ct_on ? apply_ct(s, ct) : s;
        }
        if (wdt_feed && (py & 63) == 0) {
            wdt_feed();
        }
    }
    esp_cache_msync(present, (size_t)s_pw * s_ph * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

/* Scaled (pre-rotation) buffer dimensions. scaleX always applies to the screen-
 * horizontal axis, so for 90/270 (which swap axes at display) the factors are
 * assigned to the buffer axes that become horizontal/vertical after rotation. */
static void scaled_dims(int rot, int iw, int ih, float sx, float sy, int *bw, int *bh)
{
    if (rot == 90 || rot == 270) {
        *bw = (int)lroundf((float)iw * sy);
        *bh = (int)lroundf((float)ih * sx);
    } else {
        *bw = (int)lroundf((float)iw * sx);
        *bh = (int)lroundf((float)ih * sy);
    }
    if (*bw < 1) *bw = 1;
    if (*bh < 1) *bh = 1;
}

esp_err_t image_transform_render(const uint16_t *active, const image_frame_meta_t *meta,
                                 const image_transform_t *tf, int color_temp,
                                 uint32_t generation, uint16_t *present,
                                 void (*wdt_feed)(void))
{
    if (!s_scaled || !active || !meta || !tf || !present) {
        return ESP_ERR_INVALID_ARG;
    }

    int iw = meta->width, ih = meta->height, stride = meta->stride_px;
    if (iw <= 0 || ih <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    float sx = tf->scale_x, sy = tf->scale_y;
    if (sx <= 0.0f) sx = 1.0f;
    if (sy <= 0.0f) sy = 1.0f;

    /* Rotation normalized to 0/90/180/270. */
    int rot = ((int)lroundf(tf->rotation) % 360 + 360) % 360;
    rot = (rot / 90) * 90;

    ct_mult_t ct = {256, 256, 256};
    bool ct_on = (color_temp != 6500);
    if (ct_on) {
        ct = ct_mults(color_temp);
    }

    int offx = tf->offset_x, offy = tf->offset_y;

    int bw, bh;
    scaled_dims(rot, iw, ih, sx, sy, &bw, &bh);

    bool identity_scale = (fabsf(sx - 1.0f) < 1e-4f) && (fabsf(sy - 1.0f) < 1e-4f);
    if (identity_scale) {
        /* Compose straight from the active buffer (translation + rotation only). */
        compose_inverse(active, iw, ih, stride, bw, bh, rot, offx, offy,
                        &ct, ct_on, present, wdt_feed);
        return ESP_OK;
    }

    size_t scaled_bytes = (size_t)bw * bh * 2;

    if (scaled_bytes > s_scaled_cap) {
        /* Scaled image too large to materialize: software inverse-map directly. */
        compose_inverse(active, iw, ih, stride, bw, bh, rot, offx, offy,
                        &ct, ct_on, present, wdt_feed);
        return ESP_OK;
    }

    bool cache_hit = s_cache_valid && s_cache_gen == generation &&
                     s_cache_bw == bw && s_cache_bh == bh;

    if (!cache_hit) {
        esp_err_t err = image_ppa_scale(active, iw, ih, stride, s_scaled, s_scaled_cap,
                                        bw, bh);
        size_t sync = (scaled_bytes + 127) & ~(size_t)127;
        if (sync > s_scaled_cap) {
            sync = s_scaled_cap;
        }
        if (err != ESP_OK) {
            /* PPA failed: software scale into the scratch buffer (CPU write). */
            sw_scale(active, iw, ih, stride, s_scaled, bw, bh, wdt_feed);
            esp_cache_msync(s_scaled, sync, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
        } else {
            /* PPA DMA-wrote s_scaled; invalidate so CPU compose reads fresh data. */
            esp_cache_msync(s_scaled, sync, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        }
        s_cache_valid = true;
        s_cache_gen = generation;
        s_cache_bw = bw;
        s_cache_bh = bh;
    }

    compose_from_scaled(s_scaled, bw, bh, rot, offx, offy, &ct, ct_on, present, wdt_feed);
    return ESP_OK;
}
