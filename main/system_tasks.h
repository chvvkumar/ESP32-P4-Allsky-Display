#pragma once

/*
 * System-core orchestration: watchdog policy, the periodic monitor, device-health
 * init, per-iteration main-loop housekeeping, and the OTA rollback milestone.
 *
 * This module owns the boot-time wiring of the supervised main task and the
 * normative core-assignment constants (network I/O on core 0; UI, rendering, web,
 * and orchestration on core 1). It does not create the image download or HA REST
 * tasks itself (those belong to the image-pipeline and network subsystems); it
 * publishes the stack/priority/core constants they must use.
 */

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Normative core assignment (system.md 3.5). */
#define SYS_CORE_NETWORK   0
#define SYS_CORE_UI        1

/* Download / HA-REST task shape (system.md 3.2, 3.3), published for the owning
 * subsystems so all supervised tasks agree on sizing. */
#define SYS_DL_TASK_STACK  16384
#define SYS_DL_TASK_PRIO   2
#define SYS_DL_TASK_CORE   SYS_CORE_NETWORK
#define SYS_HA_TASK_STACK  16384
#define SYS_HA_TASK_PRIO   1
#define SYS_HA_TASK_CORE   SYS_CORE_NETWORK

/* Initialize the task watchdog (configured timeout, panic+reboot action, calling
 * task subscribed), the periodic system monitor, and the device-health analyzer,
 * in that order. Call once from the main task after app_config_init() and
 * crash_log_init(). On watchdog-init failure the caller MUST halt rather than run
 * an unsupervised loop (system.md 1.6/11.3). */
esp_err_t system_core_init(void);

/* Per-main-loop housekeeping (system.md 2.5): force-feed the watchdog and advance
 * the system monitor. Call once per iteration. */
void system_core_loop_tick(void);

/* Mark the running OTA image valid to cancel the rollback arm-window. Call once
 * from the main task after the first supervised loop iteration completes
 * (system.md 10.5). Idempotent; a no-op if the image is not pending verification. */
void system_core_mark_image_valid(void);

#ifdef __cplusplus
}
#endif
