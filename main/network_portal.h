#pragma once

/**
 * @file network_portal.h
 * @brief First-boot captive portal: open AP "AllSky-Display-Setup", wildcard DNS
 * (every name resolves to the AP IP), and an HTTP setup server on port 80.
 *
 * The credential submission tests the join BEFORE persisting: on success it saves
 * the credentials, sets the provisioned flag, and signals a reboot; on failure it
 * returns an error to the form and keeps the portal up (fix for the legacy
 * always-reboot behavior). The device is expected to reboot into normal station
 * mode once network_portal_is_configured() reports true.
 */

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Bring up AP + DNS + HTTP setup server. Idempotent while running. */
esp_err_t network_portal_start(void);

/** Tear down the HTTP server, DNS task, and AP. */
void network_portal_stop(void);

/** True while the portal is running. */
bool network_portal_is_running(void);

/** True once credentials have been tested-good and persisted; the caller should
 * show a success message, stop the portal, and reboot. */
bool network_portal_is_configured(void);

#ifdef __cplusplus
}
#endif
