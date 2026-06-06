// test/test_moon_ephemeris.c
#include "../moon_ephemeris.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// time_t (UTC) for a date: uses timegm-equivalent via a fixed table is overkill;
// these epochs are precomputed UTC seconds for known lunar events.
// 2025-01-13 22:27 UTC = Full Moon  -> epoch 1736807220
// 2025-01-29 12:36 UTC = New  Moon  -> epoch 1738154160
int main(void) {
    moon_state_t full, neu;
    moon_compute((time_t)1736807220, 0.0, 0.0, &full);
    moon_compute((time_t)1738154160, 0.0, 0.0, &neu);

    printf("full illum=%.3f cycle=%.3f waxing=%d\n", full.illum, full.cycle, full.waxing);
    printf("new  illum=%.3f cycle=%.3f waxing=%d\n", neu.illum, neu.cycle, neu.waxing);

    int ok = 1;
    if (full.illum < 0.95f) { printf("FAIL: full moon illum too low\n"); ok = 0; }
    if (neu.illum  > 0.05f) { printf("FAIL: new moon illum too high\n");  ok = 0; }
    if (ok) { printf("PASS\n"); return 0; }
    return 1;
}
