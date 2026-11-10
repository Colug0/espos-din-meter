/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * espos-din-meter: application entry. espOS brings up WiFi, the config store
 * and web UI, Signal K, mDNS, logs, core dump, signed OTA and the health
 * watchdog. This file adds the DIN Meter's panel and encoder, the page store
 * and its API, and the runtime that shows the pages.
 */
#include <stdbool.h>

#include "dinmeter_hal/board.h"
#include "dinmeter_hal/display.h"
#include "dinmeter_hal/input.h"
#include "dinmeter_pages/pages.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "espos.h"
#include "espos_cfg_keys.h"
#include "espos_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brightness.h"
#include "darkmode.h"
#include "link_state.h"
#include "sound.h"
#include "lvgl.h"
#include "runtime_internal.h"

static const char *TAG = "din_meter";
static bool s_panel_up;

/* No PSRAM: internal RAM is all there is. Its low-water mark is the number
 * that predicts trouble. */
static void log_heap(const char *stage)
{
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_LOGI(TAG, "heap %-8s %6u free, %6u largest, %6u min", stage, (unsigned)heap_caps_get_free_size(caps),
             (unsigned)heap_caps_get_largest_free_block(caps), (unsigned)heap_caps_get_minimum_free_size(caps));
}

/* Stack reserve (smallest free ever, bytes) of this firmware's own tasks and
 * the LVGL pool's peak use: the numbers to size them from. */
static void log_budget(void)
{
    static const char *const tasks[] = {"dinmeter_rt", "din_sound", "taskLVGL", "main"};
    char line[128];
    int len = 0;
    for (size_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++) {
        TaskHandle_t h = xTaskGetHandle(tasks[i]);
        if (h) {
            len += snprintf(line + len, sizeof(line) - len, " %s %u", tasks[i],
                            (unsigned)uxTaskGetStackHighWaterMark(h));
        }
    }
    ESP_LOGI(TAG, "stack reserve:%s", line);
    if (s_panel_up) {
        lv_mem_monitor_t mon;
        lvgl_port_lock(0);
        lv_mem_monitor(&mon);
        lvgl_port_unlock();
        ESP_LOGI(TAG, "lvgl pool %u used of %u, peak %u, frag %u%%", (unsigned)(mon.total_size - mon.free_size),
                 (unsigned)mon.total_size, (unsigned)mon.max_used, (unsigned)mon.frag_pct);
    }
}

/* The ESP32 restarts without deauthenticating, and a MikroTik hAP ax S
 * (RouterOS 7.24) then keeps the old station and delivers no unicast to the
 * device for minutes. espOS drops the first association of each boot when
 * wifi.fresh_assoc is on (espOS #289); the boat this was built for needs it,
 * so it is on unless the owner has chosen. Costs about two seconds a boot. */
static void default_fresh_assoc(void)
{
    if (!espos_config_is_set(ESPOS_CFG_NS_WIFI, ESPOS_CFG_WIFI_FRESH_ASSOC)) {
        esp_err_t err = espos_config_set_bool(ESPOS_CFG_NS_WIFI, ESPOS_CFG_WIFI_FRESH_ASSOC, true);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "wifi.fresh_assoc not turned on: %s", esp_err_to_name(err));
        }
    }
}

/* espos_start()'s before_network hook: NVS and config are up. */
static esp_err_t board_up(void *arg)
{
    log_heap("boot");
    default_fresh_assoc();
    ESP_RETURN_ON_ERROR(dinmeter_pages_init(), TAG, "pages");
    /* Without the panel the device still reaches the network, so its logs
     * can be read over WiFi. */
    esp_err_t err = dinmeter_display_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init failed (%s), running headless", esp_err_to_name(err));
    } else {
        s_panel_up = true;
    }
    err = dinmeter_input_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "encoder init failed (%s)", esp_err_to_name(err));
    }
    /* After the display: switching dark mode off on load sets the backlight. */
    ESP_RETURN_ON_ERROR(darkmode_init(), TAG, "dark mode");
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(link_state_init());

    espos_start_opts_t opts = ESPOS_START_OPTS_DEFAULT;
    opts.app_name = "din-meter";
    opts.board = DINMETER_BOARD_NAME;
    opts.before_network = board_up;
    ESP_ERROR_CHECK(espos_start(&opts));
    log_heap("espos");

    ESP_ERROR_CHECK(dinmeter_pages_register_api());
    if (s_panel_up) {
        /* Brightness first: the runtime lights the panel after its first frame. */
        ESP_ERROR_CHECK(brightness_start());
        ESP_ERROR_CHECK(dinmeter_runtime_start());
    }
    /* A buzzer that cannot start costs the chirps, nothing else. */
    esp_err_t err = sound_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sound: %s", esp_err_to_name(err));
    }
    log_heap("runtime");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        log_heap("running");
        log_budget();
        if (s_panel_up) {
            brightness_tick();
        }
    }
}
