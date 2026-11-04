/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "sound.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "dinmeter_hal/buzzer.h"
#include "dinmeter_hal/input.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "espos_cfg_keys.h"
#include "espos_config.h"
#include "espos_tone.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Each parsed tune is about 290 bytes of internal RAM, and this board has no
 * PSRAM: a handful is plenty for event chirps. */
#define MAX_TONES 6
#define TONES_JSON_MAX 2000
#define TICK_MS 10
#define DEBOUNCE_TICKS 3

static const char *TAG = "din_sound";
static espos_tone_t s_tones[MAX_TONES];
static size_t s_n_tones;
static espos_tone_player_t s_player;
static atomic_bool s_reload;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* Sound task only. */
static void reload_tones(void)
{
    char *json = malloc(TONES_JSON_MAX);
    if (!json) {
        return;
    }
    json[0] = '\0';
    espos_config_get_str(ESPOS_CFG_NS_SOUND, ESPOS_CFG_SOUND_TONES, json, TONES_JSON_MAX, NULL);
    s_n_tones = espos_tone_library_parse(json, s_tones, MAX_TONES);
    free(json);
    ESP_LOGI(TAG, "%u tune(s)", (unsigned)s_n_tones);
}

static void play(const char *setting_key)
{
    char name[ESPOS_TONE_NAME_MAX + 1] = "";
    espos_config_get_str(ESPOS_CFG_NS_SOUND, setting_key, name, sizeof(name), NULL);
    int i = espos_tone_library_find(s_tones, s_n_tones, name);
    if (i >= 0) {
        espos_tone_player_start(&s_player, &s_tones[i], now_ms());
    }
}

/* Writer's task (httpd): only flag it, the sound task reparses. */
static void on_config_change(const char *ns, const char *key, void *arg)
{
    if (strcmp(ns, ESPOS_CFG_NS_SOUND) == 0) {
        atomic_store(&s_reload, true);
    }
}

static void sound_task(void *arg)
{
    bool pressed = false;
    int stable = 0;
    reload_tones();
    play(ESPOS_CFG_SOUND_BOOT_TONE);

    for (;;) {
        if (atomic_exchange(&s_reload, false)) {
            reload_tones();
        }
        bool down = dinmeter_input_button_pressed();
        stable = down == pressed ? 0 : stable + 1;
        if (stable >= DEBOUNCE_TICKS) {
            pressed = down;
            stable = 0;
            if (pressed) {
                play(ESPOS_CFG_SOUND_BUTTON_TONE);
            }
        }
        uint16_t hz = 0;
        if (!espos_tone_player_step(&s_player, now_ms(), &hz)) {
            hz = 0;
        }
        dinmeter_buzzer_tone(hz);
        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

esp_err_t sound_start(void)
{
    esp_err_t err = dinmeter_buzzer_init();
    if (err != ESP_OK) {
        return err;
    }
    err = espos_config_subscribe(on_config_change, NULL);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(sound_task, "din_sound", 3072, NULL, 4, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
