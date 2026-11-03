/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "runtime_internal.h"

#include "dinmeter_hal/display.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "din_renderer";

static lv_obj_t *s_label_top;

/* Single-value layout. */
static lv_obj_t *s_label_value;
static lv_obj_t *s_label_unit;

/* Dual-value layout: two side-by-side columns, each caption/value/unit. */
static lv_obj_t *s_dual_caption[2];
static lv_obj_t *s_dual_value[2];
static lv_obj_t *s_dual_unit[2];

static lv_color_t color_for_state(slot_state_t state)
{
    switch (state) {
    case SLOT_STALE:
        return lv_color_hex(0x808080);
    case SLOT_TYPE_ERROR:
        return lv_color_hex(0xFF4040);
    case SLOT_NO_DATA:
        return lv_color_hex(0x606060);
    default:
        return lv_color_white();
    }
}

static const char *text_for_slot(const dinvalue_slot_t *slot)
{
    switch (slot->state) {
    case SLOT_NO_DATA:
        return "...";
    case SLOT_TYPE_ERROR:
        /* Short on purpose: a longer word at 48 pt is wider than a dual-page
         * column. The labels are also clipped to a fixed width. */
        return "XX";
    default:
        return slot->text;
    }
}

/* The panel holds whatever was in its RAM at power-up; light it only once
 * the first page is drawn. Caller holds the LVGL lock. */
static void light_after_first_frame(void)
{
    static bool s_lit;
    if (!s_lit) {
        lv_refr_now(NULL);
        dinmeter_display_backlight_on();
        s_lit = true;
    }
}

static void set_single_visible(bool visible)
{
    lv_obj_set_hidden(s_label_value, !visible);
    lv_obj_set_hidden(s_label_unit, !visible);
}

static void set_dual_visible(bool visible)
{
    for (int i = 0; i < 2; i++) {
        lv_obj_set_hidden(s_dual_caption[i], !visible);
        lv_obj_set_hidden(s_dual_value[i], !visible);
        lv_obj_set_hidden(s_dual_unit[i], !visible);
    }
}

esp_err_t renderer_init(void)
{
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&port_cfg), TAG, "lvgl_port_init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = dinmeter_display_io(),
        .panel_handle = dinmeter_display_panel(),
        /* Small and single: the UI redraws only on a page switch or a new
         * value, and the board has no PSRAM to spare for a second buffer. */
        .buffer_size = DINMETER_DISPLAY_NATIVE_W * 15,
        .double_buffer = false,
        .hres = DINMETER_DISPLAY_NATIVE_H, /* landscape: axes swapped below */
        .vres = DINMETER_DISPLAY_NATIVE_W,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        /* M5GFX's rotation 1 for this panel (offset_rotation 2). */
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
        },
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        return ESP_FAIL;
    }

    lvgl_port_lock(0);
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    s_label_top = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_top, lv_color_white(), 0);
    lv_obj_align(s_label_top, LV_ALIGN_TOP_MID, 0, 4);

    s_label_value = lv_label_create(scr);
    lv_obj_set_style_text_font(s_label_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_label_value, lv_color_white(), 0);
    /* Fixed width and clipped, so a long string cannot run into the unit. */
    lv_obj_set_width(s_label_value, 230);
    lv_obj_set_style_text_align(s_label_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_label_value, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_align(s_label_value, LV_ALIGN_CENTER, 0, 0);

    s_label_unit = lv_label_create(scr);
    lv_obj_set_style_text_color(s_label_unit, lv_color_white(), 0);
    lv_obj_align(s_label_unit, LV_ALIGN_BOTTOM_MID, 0, -4);

    /* Dual pages have no title row: each value's caption names it. */
    static const int col_x[2] = {-60, 60};
    for (int i = 0; i < 2; i++) {
        s_dual_caption[i] = lv_label_create(scr);
        lv_obj_set_style_text_color(s_dual_caption[i], lv_color_white(), 0);
        lv_obj_align(s_dual_caption[i], LV_ALIGN_TOP_MID, col_x[i], 6);

        s_dual_value[i] = lv_label_create(scr);
        lv_obj_set_style_text_font(s_dual_value[i], &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(s_dual_value[i], lv_color_white(), 0);
        /* Fixed width and clipped, so the two columns never overlap. */
        lv_obj_set_width(s_dual_value[i], 110);
        lv_obj_set_style_text_align(s_dual_value[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(s_dual_value[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_align(s_dual_value[i], LV_ALIGN_CENTER, col_x[i], 2);

        s_dual_unit[i] = lv_label_create(scr);
        lv_obj_set_style_text_color(s_dual_unit[i], lv_color_white(), 0);
        lv_obj_align(s_dual_unit[i], LV_ALIGN_BOTTOM_MID, col_x[i], -4);
    }
    set_dual_visible(false);
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL display ready (landscape %dx%d)", DINMETER_DISPLAY_NATIVE_H, DINMETER_DISPLAY_NATIVE_W);
    return ESP_OK;
}

void renderer_show_page(const dinpage_t *page, const dinvalue_slot_t *slot)
{
    lvgl_port_lock(0);
    set_dual_visible(false);
    set_single_visible(true);
    lv_obj_set_hidden(s_label_top, false);
    lv_label_set_text(s_label_top, page->label);
    lv_obj_set_style_text_color(s_label_value, color_for_state(slot->state), 0);
    lv_label_set_text(s_label_value, text_for_slot(slot));
    lv_label_set_text(s_label_unit, page->values[0].type == DP_NUMBER ? page->values[0].unit : "");
    light_after_first_frame();
    lvgl_port_unlock();
}

void renderer_show_dual_page(const dinpage_t *page, const dinvalue_slot_t slots[2])
{
    lvgl_port_lock(0);
    set_single_visible(false);
    set_dual_visible(true);
    lv_obj_set_hidden(s_label_top, true);
    for (int i = 0; i < 2; i++) {
        lv_label_set_text(s_dual_caption[i], page->values[i].label);
        lv_obj_set_style_text_color(s_dual_value[i], color_for_state(slots[i].state), 0);
        lv_label_set_text(s_dual_value[i], text_for_slot(&slots[i]));
        lv_label_set_text(s_dual_unit[i], page->values[i].type == DP_NUMBER ? page->values[i].unit : "");
    }
    light_after_first_frame();
    lvgl_port_unlock();
}

void renderer_show_no_pages(void)
{
    lvgl_port_lock(0);
    set_dual_visible(false);
    set_single_visible(true);
    lv_obj_set_hidden(s_label_top, false);
    lv_obj_set_style_text_color(s_label_value, color_for_state(SLOT_NO_DATA), 0);
    lv_label_set_text(s_label_top, "");
    lv_label_set_text(s_label_value, "No pages");
    lv_label_set_text(s_label_unit, "configured");
    light_after_first_frame();
    lvgl_port_unlock();
}
