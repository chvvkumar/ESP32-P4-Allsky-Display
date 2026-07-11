#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Runtime display profile for the selected round DSI panel.
 *
 * BOTH panels are supported at runtime from a single binary. The active panel is chosen at
 * boot from persisted configuration (NVS key disp_type, app_config_get_disp_type()); the
 * Kconfig ALLSKY_PANEL_* choice is only the first-boot default. Dimension-dependent consumers
 * (image-buffer sizing, touch coordinate range, image pipeline) MUST derive from the active
 * profile returned by allsky_panel_profile() so they all track the runtime selection, fixing
 * the legacy Arduino quirk where some consumers used the compile-time default.
 *
 * The BSP keeps its own copy of the resolution / init byte (bsp_display_set_panel_type); both
 * this table and the BSP derive from the same type id fed by main.c at boot.
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

#define ALLSKY_PANEL_ROTATION  2   /* 180 degrees, per display-driver spec section 5 */

/**
 * @brief Pure lookup of the profile for a given panel type id.
 *
 * Unknown ids fall back to the type-1 (3.4" 800x800) profile. Does not change the active
 * selection.
 */
allsky_panel_profile_t allsky_panel_profile_for(int type_id);

/**
 * @brief Select the active panel profile by type id.
 *
 * Clamps/falls back to type 1 on any unknown id. Call once at boot after reading the persisted
 * disp_type, before any consumer queries allsky_panel_profile(). Stored in a module-level
 * variable; changing it after buffers are sized requires a reboot.
 */
void allsky_panel_set_type(int type_id);

/**
 * @brief Get the currently active display profile.
 *
 * Defaults to the type-1 (3.4") profile until allsky_panel_set_type() is called.
 */
allsky_panel_profile_t allsky_panel_profile(void);

#ifdef __cplusplus
}
#endif
