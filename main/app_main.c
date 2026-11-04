/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * espos-din-meter: application entry. espOS brings up WiFi, the config store
 * and web UI, Signal K, mDNS, logs, core dump, signed OTA and the health
 * watchdog. This file adds the DIN Meter's panel and encoder, the page store
 * and its API, and the runtime that shows the pages.
 */
#include <stdatomic.h>
#include <stdbool.h>

#include "dinmeter_hal/board.h"
#include "dinmeter_hal/display.h"
#include "dinmeter_hal/input.h"
#include "dinmeter_pages/pages.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "espos.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brightness.h"
#include "link_state.h"
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

/* The ESP32 restarts without deauthenticating, so the access point still
 * holds the previous association when the new one arrives. A MikroTik hAP ax
 * S (RouterOS 7.24.4) then keeps the old station state and for minutes
 * delivers nothing to the device while its own frames get through. So the
 * first association of every boot is dropped on purpose: the access point
 * forgets the old station, and espOS connects again at once as a new one.
 * Costs about two seconds. Belongs in espOS once it is handled there. */
static void on_wifi_connected(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    static atomic_bool done;
    if (atomic_exchange(&done, true)) {
        return;
    }
    ESP_LOGI(TAG, "dropping the first association so the access point starts a fresh one");
    esp_wifi_disconnect();
}

/* espos_start()'s before_network hook: NVS and config are up. */
static esp_err_t board_up(void *arg)
{
    log_heap("boot");
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
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_wifi_connected, NULL));

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
    log_heap("runtime");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        log_heap("running");
        if (s_panel_up) {
            brightness_tick();
        }
    }
}
