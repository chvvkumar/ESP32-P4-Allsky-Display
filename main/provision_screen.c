/**
 * @file provision_screen.c
 * @brief On-panel first-boot setup screen. See provision_screen.h for scope.
 */

#include "provision_screen.h"
#include "network_portal.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "prov_screen";

static lv_obj_t *s_root;

/* WIFI join payload for a phone camera / QR scanner: open network, no password.
 * Scanning it offers to join the setup AP; the captive portal then redirects to
 * the setup page. Format per the de-facto "WIFI:" QR convention. */
static void build_join_qr_payload(char *out, size_t out_len, const char *ssid)
{
    snprintf(out, out_len, "WIFI:T:nopass;S:%s;;", ssid);
}

void provision_screen_show(void)
{
    if (s_root != NULL) {
        return;
    }

    /* Runtime panel geometry: both round panels are square, so the short side is
     * the usable diameter. Content is kept within a centered safe zone. */
    int h_res = bsp_display_get_h_res();
    int v_res = bsp_display_get_v_res();
    if (h_res <= 0) h_res = 800;
    if (v_res <= 0) v_res = 800;
    int diameter = (h_res < v_res) ? h_res : v_res;

    /* QR sized to a legible fraction of the panel; the rest of the content scales
     * with the same base so the layout tracks 720 and 800 panels alike. */
    int qr_size = (diameter * 42) / 100;

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "display lock failed; setup screen not shown");
        return;
    }

    const char *ssid = network_portal_ap_ssid();
    const char *ip = network_portal_ap_ip();

    /* Full-panel black backdrop, everything centered vertically and horizontally
     * so content stays inside the round panel's central circle. */
    s_root = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, h_res, v_res);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_center(s_root);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_root, 10, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    lv_obj_t *title = lv_label_create(s_root);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *hint = lv_label_create(s_root);
    lv_label_set_text(hint, "Scan to join, then open the page");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    /* QR code: white quiet zone so it scans against the black backdrop. */
    lv_obj_t *qr = lv_qrcode_create(s_root);
    lv_qrcode_set_size(qr, qr_size);
    lv_qrcode_set_dark_color(qr, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(qr, lv_color_hex(0xFFFFFF));
    lv_qrcode_set_quiet_zone(qr, true);
    lv_obj_set_style_border_color(qr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(qr, 6, 0);

    char payload[80];
    build_join_qr_payload(payload, sizeof(payload), ssid);
    lv_qrcode_update(qr, payload, strlen(payload));

    lv_obj_t *ssid_lbl = lv_label_create(s_root);
    lv_label_set_text_fmt(ssid_lbl, "Network: %s", ssid);
    lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_align(ssid_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *url_lbl = lv_label_create(s_root);
    lv_label_set_text_fmt(url_lbl, "http://%s", ip);
    lv_obj_set_style_text_font(url_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(url_lbl, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_align(url_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *waiting = lv_label_create(s_root);
    lv_label_set_text(waiting, "Waiting for setup...");
    lv_obj_set_style_text_font(waiting, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(waiting, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_align(waiting, LV_TEXT_ALIGN_CENTER, 0);

    bsp_display_unlock();
    ESP_LOGI(TAG, "Setup screen shown: SSID '%s' at %s (%dx%d, qr %d)",
             ssid, ip, h_res, v_res, qr_size);
}

void provision_screen_hide(void)
{
    if (s_root == NULL) {
        return;
    }
    if (!bsp_display_lock(0)) {
        return;
    }
    lv_obj_del(s_root);
    s_root = NULL;
    bsp_display_unlock();
}
