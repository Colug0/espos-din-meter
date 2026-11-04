/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "brightness.h"

#include <stdatomic.h>
#include <string.h>

#include "dinmeter_hal/display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espos_cfg_keys.h"
#include "espos_config.h"
#include "espos_sk.h"
#include "darkmode.h"
#include "link_state.h"

/* A short drop at night keeps the panel dark; an outage across sunrise does
 * not keep it dimmed all morning. */
#define FORGET_MODE_MS (30 * 60 * 1000)

static const char *TAG = "din_bright";
static atomic_bool s_night;
static atomic_bool s_mode_seen;
static int64_t s_sk_lost_ms; /* brightness_tick() only */

void brightness_apply(void)
{
    int32_t day = 80;
    int32_t night = 0;
    espos_config_get_i32(ESPOS_CFG_NS_DISPLAY, ESPOS_CFG_DISPLAY_BRIGHTNESS, &day);
    espos_config_get_i32(ESPOS_CFG_NS_DISPLAY, ESPOS_CFG_DISPLAY_NIGHT_BRIGHT, &night);
    /* night_bright 0 means "no night setting": stay at the day value. */
    bool dark = atomic_load(&s_night) || darkmode_active();
    int32_t pct = dark && night > 0 ? night : day;
    dinmeter_display_set_brightness((uint8_t)pct);
}

/* Stream task: compare and return. */
static void on_mode(const espos_sk_update_t *u, void *arg)
{
    if (!u->value_json) {
        return;
    }
    bool night = strcmp(u->value_json, "\"night\"") == 0;
    bool first = !atomic_exchange(&s_mode_seen, true);
    bool was_night = atomic_exchange(&s_night, night);
    if (was_night != night || first) {
        ESP_LOGI(TAG, "environment.mode %s", u->value_json);
        /* Daylight ends dark mode: at the night-to-day change, or on the
         * first value after a restart. Not on every repeat of "day", so it
         * can still be turned on by day. */
        if (!night && (was_night || first)) {
            darkmode_set(false);
        }
        brightness_apply();
    }
}

/* Writer's task (httpd). */
static void on_config_change(const char *ns, const char *key, void *arg)
{
    if (strcmp(ns, ESPOS_CFG_NS_DISPLAY) == 0) {
        brightness_apply();
    }
}

esp_err_t brightness_start(void)
{
    brightness_apply();
    esp_err_t err = espos_config_subscribe(on_config_change, NULL);
    if (err != ESP_OK) {
        return err;
    }
    if (espos_sk_subscribe("environment.mode", 10000, on_mode, NULL) <= 0) {
        ESP_LOGW(TAG, "environment.mode not subscribed: night brightness off");
    }
    return ESP_OK;
}

void brightness_tick(void)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (link_state_sk_up()) {
        s_sk_lost_ms = 0;
        return;
    }
    if (s_sk_lost_ms == 0) {
        s_sk_lost_ms = now_ms;
    } else if (now_ms - s_sk_lost_ms > FORGET_MODE_MS && atomic_exchange(&s_night, false)) {
        ESP_LOGI(TAG, "no Signal K for 30 min: back to day brightness");
        brightness_apply();
    }
}
