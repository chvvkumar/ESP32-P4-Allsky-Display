#pragma once

/*
 * network_timeparse.h - Pure, host-testable RFC-1123 HTTP Date parser.
 *
 * No ESP-IDF dependencies; standard C only.
 */

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse an RFC-1123 HTTP Date header ("Mon, 06 Jul 2026 19:06:19 GMT")
 * to a Unix epoch timestamp (UTC).
 *
 * - The day-of-week prefix, if present, is skipped without validation.
 * - Month is a 3-letter English abbreviation (case-insensitive).
 * - A trailing "GMT" or "UTC" suffix is tolerated but not required.
 * - Single-digit days ("Mon, 6 Jul 2026 ...") are accepted.
 *
 * Returns 0 on any parse failure (including NULL/empty input).
 */
time_t time_parse_rfc1123(const char *s);

#ifdef __cplusplus
}
#endif
