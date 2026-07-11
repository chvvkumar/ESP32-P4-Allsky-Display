#pragma once

/**
 * @file network_poll_backoff.h
 * @brief Pure exponential-backoff step function for network_poll.c.
 *
 * Header-only, no ESP-IDF/FreeRTOS includes: host-testable in isolation.
 * Reset-on-success is a one-line caller responsibility (assign the backoff-state
 * variable back to 0 after a successful poll), not encoded here.
 */

#include <stdint.h>

/**
 * Compute the next backoff delay (ms) after a failed poll.
 *
 * @param cur      Current backoff delay in ms; 0 means "no failure yet".
 * @param initial  Delay used on the first failure. 0 disables backoff entirely
 *                  (always returns 0, telling the caller to use its normal
 *                  poll interval).
 * @param max      Ceiling the doubling saturates at. 0 means unbounded.
 */
static inline uint32_t network_poll_backoff_next(uint32_t cur, uint32_t initial, uint32_t max) {
    if (initial == 0) return 0;

    if (cur == 0) {
        return (max != 0 && initial > max) ? max : initial;
    }

    uint32_t next = cur * 2;
    if (next < cur) next = (max != 0) ? max : cur; /* overflow guard */
    if (max != 0 && next > max) next = max;
    return next;
}
