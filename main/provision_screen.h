#pragma once

/**
 * @file provision_screen.h
 * @brief On-panel setup screen shown while the first-boot captive portal is up.
 *
 * Builds a self-contained LVGL screen (title, AP SSID, portal URL, and a QR code
 * that joins the open setup AP) so the panel is not blank during provisioning.
 * Sized from the active panel profile so it renders on both round DSI panels.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Build and display the provisioning screen. Safe to call once; idempotent. */
void provision_screen_show(void);

/** Remove the provisioning screen if it is shown. */
void provision_screen_hide(void);

#ifdef __cplusplus
}
#endif
