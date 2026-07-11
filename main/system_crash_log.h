#pragma once

/*
 * Crash logging and boot forensics.
 *
 * Three log tiers, all fed by every log call routed through the vprintf hook:
 *   - RAM ring   (8192 B, internal heap): current session only.
 *   - RTC ring   (4096 B, RTC_NOINIT):    survives soft resets and crashes, not
 *                                          power loss. Also holds boot counter,
 *                                          ring positions and a crash marker word.
 *   - NVS blob   (namespace "crash_log", <=4000 B): survives power loss. Keys
 *                 log_data / log_boot / log_time / log_timestamp.
 *
 * Additionally, panic banner + RISC-V backtrace text is mirrored into a separate
 * RTC_NOINIT ring by __wrap_panic_print_char() (linker -Wl,--wrap=panic_print_char)
 * and attached to the forensic record on the next boot. This provides on-device
 * backtrace capture that survives the panic-triggered reset.
 */

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise the RAM/RTC rings, increment the persistent boot counter, classify
 * the previous boot (crash vs normal), record a forensic entry, and on crash
 * persist the RTC ring to NVS. Installs the vprintf capture hook. Call once early
 * in boot (after nvs_flash_init, before heavy init). */
void crash_log_init(void);

/* True if the previous boot ended in a crash (panic/watchdog reset or an explicit
 * crash marker). Valid after crash_log_init(). */
bool crash_log_last_boot_was_crash(void);

/* True if the latched reset reason was a watchdog reset (task/interrupt/generic).
 * Used to wire the device-health watchdog_resets counter. */
bool crash_log_reset_was_watchdog(void);

/* Persistent boot count (survives power loss via NVS). */
uint32_t crash_log_boot_count(void);

/* Human-readable name of the latched reset reason for this boot. */
const char *crash_log_reset_reason_name(void);

/* Explicit write into the RAM + RTC tiers. If wall time is valid (year > 2016)
 * and msg begins with '[', a "[YYYY-MM-DD HH:MM:SS] " prefix is prepended. Most
 * callers rely on the vprintf hook instead; this is for out-of-band notes. */
void crash_log_write(const char *msg);

/* Mark the current session as a crash out-of-band (sets the RTC crash marker and
 * persists). Next boot classifies as crash even if the reset reason is benign. */
void crash_log_mark_crash(void);

/* Persist the RTC ring to NVS and clear the crash marker. MUST be called before
 * every intentional reboot (user reboot, factory reset, fatal-alloc restart,
 * provisioning restart) so the log survives and the next boot is not misjudged. */
void crash_log_prepare_reboot(void);

/* Tier retrieval. Each copies up to len-1 bytes (NUL-terminated) oldest-first and
 * returns the number of bytes written. */
size_t crash_log_get_ram(char *out, size_t len);
size_t crash_log_get_rtc(char *out, size_t len);
size_t crash_log_get_nvs(char *out, size_t len);

/* Combined dump: NVS (previous boot), then RTC (since last reboot), then RAM
 * (current session), preceded by a header with current time, boot count, session
 * uptime and last-boot-crash flag. Returns bytes written. */
size_t crash_log_get_combined(char *out, size_t len);

/* Wipe all three tiers and the crash marker. */
void crash_log_clear_all(void);

#ifdef __cplusplus
}
#endif
