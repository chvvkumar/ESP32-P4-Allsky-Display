#include "input_touch.h"
#include "input_context.h"
#include "moon_interaction.h"
#include "moon_source.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "input";

/* Gesture timing tolerances (microseconds). */
#define DEBOUNCE_US            50000    /* 50 ms  */
#define MIN_TAP_US             50000    /* 50 ms  */
#define MAX_TAP_US             1000000  /* 1000 ms */
#define DOUBLE_TAP_WINDOW_US   400000   /* 400 ms */
#define POLL_PERIOD_MS         50

typedef enum {
    GS_IDLE = 0,
    GS_PRESSED,
    GS_WAIT_SECOND,
} gesture_state_t;

/* All recognizer state below is touched only from the lv_timer callback, which
 * runs single-threaded on the LVGL task. No locking needed among these fields. */
static lv_indev_t         *s_indev;
static lv_timer_t         *s_timer;
static input_gesture_hooks_t s_hooks;
static bool                s_double_tap_enabled = true;

static gesture_state_t     s_gs = GS_IDLE;
static bool                s_prev_pressed;
static int64_t             s_last_press_us;
static int64_t             s_last_release_us;
static int64_t             s_press_start_us;
static int64_t             s_first_release_us;
static bool                s_pending_first;

/* Cross-task flag (moon page). Written by the image pipeline when the displayed
 * source changes, read by the recognizer; a single aligned bool is atomic on
 * this target. */
static volatile bool       s_moon_active;

/* Moon drag edge-detection state, owned by the poll callback (LVGL task). */
static bool                s_moon_prev_active;
static bool                s_moon_prev_pressed;

/* ---- Cross-subsystem input context (input_context.h) -------------------- */

void input_set_moon_page_active(bool active)
{
    s_moon_active = active;
}

bool input_moon_page_active(void)
{
    return s_moon_active;
}

bool input_touch_read(int *x, int *y)
{
    if (!s_indev) {
        return false;
    }
    lv_point_t p;
    lv_indev_get_point(s_indev, &p);
    if (x) {
        *x = (int)p.x;
    }
    if (y) {
        *y = (int)p.y;
    }
    return lv_indev_get_state(s_indev) == LV_INDEV_STATE_PRESSED;
}

/* ---- Gesture recognizer ------------------------------------------------- */

static void emit_single(void)
{
    ESP_LOGD(TAG, "single tap");
    if (s_hooks.single_tap) {
        s_hooks.single_tap();
    }
}

static void emit_double(void)
{
    ESP_LOGD(TAG, "double tap");
    if (s_hooks.double_tap) {
        s_hooks.double_tap();
    }
}

static void handle_press(int64_t now)
{
    s_press_start_us = now;
    /* From IDLE or WAIT_SECOND both enter PRESSED. A pending first tap is kept;
     * whether its window expired is resolved at release. */
    s_gs = GS_PRESSED;
}

static void handle_release(int64_t now)
{
    if (s_gs != GS_PRESSED) {
        s_gs = GS_IDLE;
        return;
    }
    int64_t dur = now - s_press_start_us;
    if (dur < MIN_TAP_US || dur > MAX_TAP_US) {
        /* Out-of-range press: not a tap. Discard any pending first tap. */
        s_gs = GS_IDLE;
        s_pending_first = false;
        return;
    }
    if (!s_pending_first) {
        if (s_double_tap_enabled) {
            s_first_release_us = now;
            s_pending_first = true;
            s_gs = GS_WAIT_SECOND;
        } else {
            emit_single();
            s_gs = GS_IDLE;
        }
    } else {
        if (now - s_first_release_us <= DOUBLE_TAP_WINDOW_US) {
            emit_double();
        } else {
            emit_single();
        }
        s_gs = GS_IDLE;
        s_pending_first = false;
    }
}

static void poll_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_indev) {
        return;
    }

    bool pressed = lv_indev_get_state(s_indev) == LV_INDEV_STATE_PRESSED;

    /* Leaving the moon page: drop any drag / free-spin state so the next visit
     * starts from the live orientation, and let the interactive task run out. */
    if (!s_moon_active && s_moon_prev_active) {
        moon_drag_reset();
        moon_interactive_wake();
        s_moon_prev_pressed = false;
    }
    s_moon_prev_active = s_moon_active;

    /* Moon page: own the touch stream. Drive the drag-to-rotate model from the
     * finger and wake the interactive render task; keep the tap recognizer IDLE
     * so no tap advances the slideshow and leaving the page emits nothing. */
    if (s_moon_active) {
        int mx = 0, my = 0;
        bool moon_pressed = input_touch_read(&mx, &my);
        if (moon_pressed && !s_moon_prev_pressed) {
            moon_drag_begin((float)mx, (float)my);
            moon_interactive_wake();
        } else if (moon_pressed) {
            moon_drag_move((float)mx, (float)my);
            moon_interactive_wake();
        } else if (!moon_pressed && s_moon_prev_pressed) {
            moon_drag_end();
            moon_interactive_wake();
        }
        s_moon_prev_pressed = moon_pressed;

        s_gs = GS_IDLE;
        s_pending_first = false;
        s_prev_pressed = pressed;
        return;
    }

    int64_t now = esp_timer_get_time();

    if (pressed != s_prev_pressed) {
        /* Debounce: discard a transition too close to the last accepted one. */
        if (now - s_last_press_us < DEBOUNCE_US ||
            now - s_last_release_us < DEBOUNCE_US) {
            return;
        }
        s_prev_pressed = pressed;
        if (pressed) {
            s_last_press_us = now;
            handle_press(now);
        } else {
            s_last_release_us = now;
            handle_release(now);
        }
        return;
    }

    /* Steady state: resolve the double-tap window expiry. */
    if (s_gs == GS_WAIT_SECOND && s_pending_first &&
        now - s_first_release_us > DOUBLE_TAP_WINDOW_US) {
        emit_single();
        s_gs = GS_IDLE;
        s_pending_first = false;
    }
}

void input_touch_set_double_tap_enabled(bool enabled)
{
    s_double_tap_enabled = enabled;
}

esp_err_t input_touch_init(const input_gesture_hooks_t *hooks)
{
    if (hooks) {
        s_hooks = *hooks;
    }

    s_indev = bsp_display_get_input_dev();
    if (!s_indev) {
        ESP_LOGW(TAG, "no input device; touch disabled for this session");
        return ESP_OK;
    }

    if (!bsp_display_lock(1000)) {
        ESP_LOGE(TAG, "failed to take LVGL lock for timer creation");
        return ESP_ERR_TIMEOUT;
    }
    s_timer = lv_timer_create(poll_cb, POLL_PERIOD_MS, NULL);
    bsp_display_unlock();

    if (!s_timer) {
        ESP_LOGE(TAG, "failed to create gesture timer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "touch gesture recognizer started (%d ms poll)", POLL_PERIOD_MS);
    return ESP_OK;
}
