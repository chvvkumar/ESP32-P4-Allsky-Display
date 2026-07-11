#pragma once

/*
 * Boot log capture into a bounded PSRAM circular buffer.
 *
 * Hooks the ESP-IDF logging vprintf so everything printed to the console since
 * boot is also copied into a fixed-size circular byte buffer in PSRAM. The hook
 * chains to the original vprintf, so the serial console and idf.py monitor are
 * unaffected. When the buffer fills, the oldest bytes are overwritten.
 *
 * Memory is a single fixed allocation made once at init; it never grows. If the
 * PSRAM allocation fails, capture degrades to pass-through (console-only) logging.
 *
 * This ring backs the WebSocket console replay (web-server subsystem). The crash
 * logger (system_crash_log.h) keeps its own smaller RAM/RTC tiers for forensics.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fixed circular buffer size: 512 KB, PSRAM-allocated. */
#define LOG_CAPTURE_SIZE (512 * 1024)

/* Allocate the PSRAM ring and install the vprintf hook. Safe to call once early
 * in boot. On allocation failure logs one warning and leaves logging intact. */
void log_capture_init(void);

/* vprintf hook installed via esp_log_set_vprintf(). Exposed so a later hook can
 * chain to it explicitly if needed. */
int log_capture_vprintf(const char *fmt, va_list args);

/* Copy up to max_len bytes of captured log, oldest-first, into dst starting at
 * logical byte offset (0 = oldest valid byte). Returns bytes copied (0 at end).
 * Callers stream by advancing offset. */
size_t log_capture_read(size_t offset, char *dst, size_t max_len);

/* Reset the ring to empty. */
void log_capture_clear(void);

/* Current number of valid (readable) bytes in the ring. */
size_t log_capture_size(void);

/* Total bytes ever appended since boot (monotonic; NOT reset by log_capture_clear).
 * The oldest still-readable byte is at absolute position (total - size). A live
 * consumer keeps an absolute cursor and reads new bytes as: oldest = total - size;
 * if cursor < oldest, cursor = oldest (bytes were overwritten); then read at
 * logical offset (cursor - oldest). */
uint64_t log_capture_total(void);

#ifdef __cplusplus
}
#endif
