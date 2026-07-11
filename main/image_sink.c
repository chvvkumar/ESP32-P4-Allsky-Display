/**
 * @file image_sink.c
 * @brief LVGL sink: full-screen image widget fed by the transform stage.
 */

#include "image_sink.h"
#include "display_defs.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "img_sink";

static lv_obj_t       *s_img;
static lv_obj_t       *s_badge;
static lv_image_dsc_t  s_dsc;
static bool            s_has_image;

esp_err_t image_sink_create(void)
{
    lv_obj_t *scr = lv_screen_active();
    if (!scr) {
        return ESP_FAIL;
    }
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    memset(&s_dsc, 0, sizeof(s_dsc));
    s_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_dsc.header.cf = LV_COLOR_FORMAT_RGB565;

    s_img = lv_image_create(scr);
    lv_obj_set_pos(s_img, 0, 0);
    lv_image_set_pivot(s_img, 0, 0);
    lv_obj_add_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_img, LV_OBJ_FLAG_SCROLLABLE);

    /* Stale-image badge: small chip, top-right, hidden by default. */
    s_badge = lv_label_create(scr);
    lv_label_set_text(s_badge, "STALE");
    lv_obj_set_style_text_color(s_badge, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_badge, lv_color_hex(0xB00020), 0);
    lv_obj_set_style_bg_opa(s_badge, LV_OPA_80, 0);
    lv_obj_set_style_pad_left(s_badge, 8, 0);
    lv_obj_set_style_pad_right(s_badge, 8, 0);
    lv_obj_set_style_pad_top(s_badge, 4, 0);
    lv_obj_set_style_pad_bottom(s_badge, 4, 0);
    lv_obj_set_style_radius(s_badge, 6, 0);
    lv_obj_align(s_badge, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);

    s_has_image = false;
    ESP_LOGI(TAG, "image sink created");
    return ESP_OK;
}

void image_sink_present(const uint16_t *buf, int w, int h)
{
    if (!s_img || !buf || w <= 0 || h <= 0) {
        return;
    }
    s_dsc.data = (const uint8_t *)buf;
    s_dsc.data_size = (size_t)w * h * 2;
    s_dsc.header.w = w;
    s_dsc.header.h = h;
    s_dsc.header.stride = w * 2;

    lv_image_set_src(s_img, &s_dsc);
    lv_image_set_scale(s_img, 256); /* 1:1, buffer is already panel-sized */
    lv_obj_clear_flag(s_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(s_img);
    s_has_image = true;
}

void image_sink_set_stale(bool stale)
{
    if (!s_badge) {
        return;
    }
    if (stale) {
        lv_obj_clear_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_badge);
    } else {
        lv_obj_add_flag(s_badge, LV_OBJ_FLAG_HIDDEN);
    }
}

bool image_sink_has_image(void)
{
    return s_has_image;
}

lv_obj_t *image_sink_widget(void)
{
    return s_img;
}
