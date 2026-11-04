/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "darkmode.h"

#include <stdatomic.h>
#include <string.h>

#include "brightness.h"
#include "esp_log.h"
#include "espos_cfg_keys.h"
#include "espos_config.h"
#include "nvs.h"

#define NVS_NAMESPACE "dinmeter"
#define NVS_KEY "dark"

static const char *TAG = "din_dark";
static atomic_bool s_active;
static atomic_bool s_enabled;
static atomic_uint s_timeout_s = 15;

static void load_settings(void)
{
    bool enabled = false;
    int32_t timeout_s = 15;
    espos_config_get_bool(ESPOS_CFG_NS_DISPLAY, ESPOS_CFG_DISPLAY_DARK_ENABLED, &enabled);
    espos_config_get_i32(ESPOS_CFG_NS_DISPLAY, ESPOS_CFG_DISPLAY_DARK_TIMEOUT_S, &timeout_s);
    atomic_store(&s_enabled, enabled);
    atomic_store(&s_timeout_s, (unsigned)timeout_s);
    /* Forbidding it ends it: nothing else could, short of daylight. */
    if (!enabled && atomic_load(&s_active)) {
        darkmode_set(false);
    }
}

/* Writer's task (httpd). */
static void on_config_change(const char *ns, const char *key, void *arg)
{
    if (strcmp(ns, ESPOS_CFG_NS_DISPLAY) == 0) {
        load_settings();
    }
}

esp_err_t darkmode_init(void)
{
    nvs_handle_t h;
    uint8_t stored = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, NVS_KEY, &stored);
        nvs_close(h);
    }
    atomic_store(&s_active, stored != 0);
    load_settings();
    if (atomic_load(&s_active)) {
        ESP_LOGI(TAG, "dark mode on (stored)");
    }
    return espos_config_subscribe(on_config_change, NULL);
}

bool darkmode_enabled(void)
{
    return atomic_load(&s_enabled);
}

bool darkmode_active(void)
{
    return atomic_load(&s_active);
}

uint32_t darkmode_timeout_ms(void)
{
    return atomic_load(&s_timeout_s) * 1000u;
}

void darkmode_set(bool on)
{
    if (atomic_exchange(&s_active, on) == on) {
        return;
    }
    ESP_LOGI(TAG, "dark mode %s", on ? "on" : "off");
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY, on ? 1 : 0);
        nvs_commit(h);
        nvs_close(h);
    }
    brightness_apply();
}
