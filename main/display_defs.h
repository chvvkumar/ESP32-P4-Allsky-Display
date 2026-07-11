#pragma once

#include <stdint.h>
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time display profile for the selected round DSI panel.
 *
 * Panel selection is the single Kconfig choice ALLSKY_PANEL_* (main/Kconfig.projbuild),
 * which also drives the BSP resolution and the JD9365 init byte. This header exposes the
 * same selection to the rest of the firmware as a small profile struct so dimension-dependent
 * consumers (image-buffer sizing, touch coordinate range, image pipeline) all derive from ONE
 * source, fixing the legacy Arduino quirk where some consumers used the compile-time default.
 *
 * See .planning/specs/display-driver.md for the authoritative hardware table.
 */

typedef struct {
    int         type_id;      /*!< Persisted displayType id: 1 = 3.4", 2 = 4.0" */
    const char *name;         /*!< Profile name string, e.g. "3.4INCH-DSI" */
    uint16_t    width;        /*!< Active width in pixels (post-rotation; square panels) */
    uint16_t    height;       /*!< Active height in pixels */
    uint8_t     panel_init_byte; /*!< JD9365 page-1 reg 0x40 value for this panel */
    uint8_t     rotation;     /*!< Rotation index (2 = 180 degrees) */
} allsky_panel_profile_t;

#if CONFIG_ALLSKY_PANEL_4_0
#define ALLSKY_PANEL_TYPE_ID   2
#define ALLSKY_PANEL_NAME      "4INCH-DSI"
#define ALLSKY_PANEL_WIDTH     720
#define ALLSKY_PANEL_HEIGHT    720
#define ALLSKY_PANEL_INIT_BYTE 0x04
#else
#define ALLSKY_PANEL_TYPE_ID   1
#define ALLSKY_PANEL_NAME      "3.4INCH-DSI"
#define ALLSKY_PANEL_WIDTH     800
#define ALLSKY_PANEL_HEIGHT    800
#define ALLSKY_PANEL_INIT_BYTE 0x00
#endif

#define ALLSKY_PANEL_ROTATION  2   /* 180 degrees, per display-driver spec section 5 */

/**
 * @brief Get the active display profile for the panel selected at build time.
 */
static inline allsky_panel_profile_t allsky_panel_profile(void)
{
    allsky_panel_profile_t p = {
        .type_id = ALLSKY_PANEL_TYPE_ID,
        .name = ALLSKY_PANEL_NAME,
        .width = ALLSKY_PANEL_WIDTH,
        .height = ALLSKY_PANEL_HEIGHT,
        .panel_init_byte = ALLSKY_PANEL_INIT_BYTE,
        .rotation = ALLSKY_PANEL_ROTATION,
    };
    return p;
}

#ifdef __cplusplus
}
#endif
