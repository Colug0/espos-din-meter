/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "dinmeter_hal/input.h"
#include "dinmeter_pages/conversions.h"
#include "dinmeter_pages/pages.h"
#include "espos_sk.h"
#include "darkmode.h"
#include "dinmeter_hal/display.h"
#include "link_state.h"
#include "runtime_internal.h"

static const char *TAG = "din_runtime";

static SemaphoreHandle_t s_lock;
static dinpage_t s_pages[DINMETER_PAGES_MAX];
static struct {
    dinvalue_slot_t slot;
    int64_t updated_at_ms;
} s_value_state[DINMETER_PAGES_MAX][DINPAGE_MAX_VALUES];
static int s_sub_handles[DINMETER_PAGES_MAX][DINPAGE_MAX_VALUES];
static int s_page_count;

static dinpage_type_t parse_type(const char *s)
{
    if (!s) {
        return DP_UNKNOWN;
    }
    if (strcmp(s, "number") == 0) {
        return DP_NUMBER;
    }
    if (strcmp(s, "string") == 0) {
        return DP_STRING;
    }
    if (strcmp(s, "boolean") == 0) {
        return DP_BOOLEAN;
    }
    return DP_UNKNOWN;
}

static void copy_str_field(char *dst, size_t dst_size, const cJSON *obj, const char *key)
{
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    snprintf(dst, dst_size, "%s", cJSON_IsString(item) ? item->valuestring : "");
}

/* A single page itself, or one entry of a dual page's "values". */
static void parse_value(dinvalue_t *val, const cJSON *obj)
{
    memset(val, 0, sizeof(*val));
    copy_str_field(val->label, sizeof(val->label), obj, "label");
    copy_str_field(val->path, sizeof(val->path), obj, "path");
    char type_buf[16];
    copy_str_field(type_buf, sizeof(type_buf), obj, "type");
    val->type = parse_type(type_buf);
    copy_str_field(val->unit, sizeof(val->unit), obj, "unit");

    cJSON *dec = cJSON_GetObjectItem(obj, "decimals");
    val->decimals = cJSON_IsNumber(dec) ? dec->valueint : 0;

    char conv_buf[24];
    copy_str_field(conv_buf, sizeof(conv_buf), obj, "conversion");
    const dinmeter_conversion_t *conv = dinmeter_conversion_find(conv_buf);
    val->conv_factor = conv ? conv->factor : 1.0;
    val->conv_offset = conv ? conv->offset : 0.0;
    if (conv) {
        snprintf(val->unit, sizeof(val->unit), "%s", conv->unit);
    }
}

/* Runs once at start and then on the httpd task after each saved change. A
 * change is a rare owner action, so everything is unsubscribed and
 * subscribed again rather than diffed. */
static void resync_pages_and_subscriptions(void *arg);

static void on_sk_update(const espos_sk_update_t *u, void *arg)
{
    int enc = (int)(intptr_t)arg;
    int page_idx = enc / DINPAGE_MAX_VALUES;
    int value_idx = enc % DINPAGE_MAX_VALUES;
    if (!u->value_json) {
        return; /* meta-only update, nothing to display */
    }
    /* A small scalar: cheap to parse on the stream task, and cJSON gets the
     * string escapes right. */
    cJSON *v = cJSON_Parse(u->value_json);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (page_idx < 0 || page_idx >= s_page_count || value_idx >= s_pages[page_idx].value_count) {
        xSemaphoreGive(s_lock);
        cJSON_Delete(v);
        return;
    }
    dinvalue_t *val = &s_pages[page_idx].values[value_idx];
    dinvalue_slot_t *slot = &s_value_state[page_idx][value_idx].slot;
    dinpage_type_t sniffed = DP_UNKNOWN;
    if (cJSON_IsString(v)) {
        sniffed = DP_STRING;
    } else if (cJSON_IsBool(v)) {
        sniffed = DP_BOOLEAN;
    } else if (cJSON_IsNumber(v)) {
        sniffed = DP_NUMBER;
    }

    if (sniffed != val->type) {
        slot->state = SLOT_TYPE_ERROR;
    } else {
        switch (val->type) {
        case DP_NUMBER: {
            double shown = v->valuedouble * val->conv_factor + val->conv_offset;
            snprintf(slot->text, sizeof(slot->text), "%.*f", val->decimals, shown);
            break;
        }
        case DP_STRING:
            snprintf(slot->text, sizeof(slot->text), "%s", v->valuestring);
            break;
        case DP_BOOLEAN:
            snprintf(slot->text, sizeof(slot->text), "%s", cJSON_IsTrue(v) ? "true" : "false");
            break;
        default:
            break;
        }
        slot->state = SLOT_OK;
        s_value_state[page_idx][value_idx].updated_at_ms = esp_timer_get_time() / 1000;
    }
    xSemaphoreGive(s_lock);
    cJSON_Delete(v);
}

static void resync_pages_and_subscriptions(void *arg)
{
    (void)arg;
    char *json = dinmeter_pages_get_json();
    if (!json) {
        ESP_LOGE(TAG, "resync: out of memory reading pages");
        return;
    }
    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) {
        ESP_LOGE(TAG, "resync: stored pages document is not valid JSON");
        return;
    }

    for (int i = 0; i < s_page_count; i++) {
        for (int v = 0; v < s_pages[i].value_count; v++) {
            if (s_sub_handles[i][v] > 0) {
                espos_sk_unsubscribe(s_sub_handles[i][v]);
            }
            s_sub_handles[i][v] = 0;
        }
    }

    cJSON *pages = cJSON_GetObjectItem(root, "pages");
    int n = cJSON_IsArray(pages) ? cJSON_GetArraySize(pages) : 0;
    if (n > DINMETER_PAGES_MAX) {
        n = DINMETER_PAGES_MAX;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_page_count = n;
    for (int i = 0; i < n; i++) {
        cJSON *p = cJSON_GetArrayItem(pages, i);
        dinpage_t *dp = &s_pages[i];
        memset(dp, 0, sizeof(*dp));
        copy_str_field(dp->id, sizeof(dp->id), p, "id");
        copy_str_field(dp->label, sizeof(dp->label), p, "label");

        char kind_buf[8];
        copy_str_field(kind_buf, sizeof(kind_buf), p, "kind");
        bool is_dual = strcmp(kind_buf, "dual") == 0;

        if (is_dual) {
            cJSON *values = cJSON_GetObjectItem(p, "values");
            dp->value_count = 2;
            for (int v = 0; v < 2; v++) {
                parse_value(&dp->values[v], cJSON_GetArrayItem(values, v));
            }
        } else {
            dp->value_count = 1;
            parse_value(&dp->values[0], p);
        }

        cJSON *period = cJSON_GetObjectItem(p, "updatePeriodMs");
        dp->update_period_ms = cJSON_IsNumber(period) ? (uint32_t)period->valuedouble : 1000;
        cJSON *stale = cJSON_GetObjectItem(p, "staleTimeoutMs");
        dp->stale_timeout_ms = cJSON_IsNumber(stale) ? (uint32_t)stale->valuedouble : dp->update_period_ms * 5;

        for (int v = 0; v < dp->value_count; v++) {
            s_value_state[i][v].slot.state = SLOT_NO_DATA;
            s_value_state[i][v].slot.text[0] = '\0';
            s_value_state[i][v].updated_at_ms = 0;
        }
    }
    xSemaphoreGive(s_lock);
    cJSON_Delete(root);

    for (int i = 0; i < n; i++) {
        for (int v = 0; v < s_pages[i].value_count; v++) {
            int enc = i * DINPAGE_MAX_VALUES + v;
            int h = espos_sk_subscribe(s_pages[i].values[v].path, s_pages[i].update_period_ms,
                                        on_sk_update, (void *)(intptr_t)enc);
            s_sub_handles[i][v] = h > 0 ? h : 0;
            if (h <= 0) {
                ESP_LOGW(TAG, "subscribe failed for page %d value %d (%s)", i, v, s_pages[i].values[v].path);
            }
        }
    }
    ESP_LOGI(TAG, "resynced %d page(s)", n);
}

/* The slot's state with the stale timeout applied. Caller holds s_lock. */
static slot_state_t effective_state(int page_idx, int value_idx, uint32_t stale_timeout_ms)
{
    slot_state_t state = s_value_state[page_idx][value_idx].slot.state;
    if (state == SLOT_OK) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - s_value_state[page_idx][value_idx].updated_at_ms > (int64_t)stale_timeout_ms) {
            return SLOT_STALE;
        }
    }
    return state;
}

static bool slots_equal(const dinvalue_slot_t *a, const dinvalue_slot_t *b)
{
    return a->state == b->state && strcmp(a->text, b->text) == 0;
}

#define LONG_PRESS_MS 1000
#define TOAST_MS 1500
#define DEBOUNCE_POLLS 2

/* A short confirmation on screen, e.g. after a long press. Runtime task. */
static struct {
    int64_t until_ms;
    const char *main;
    const char *hint;
} s_toast;

static void toast(int64_t now_ms, const char *main, const char *hint)
{
    s_toast.until_ms = now_ms + TOAST_MS;
    s_toast.main = main;
    s_toast.hint = hint;
}

/* A toast, then setup and approval, which are what an owner meets first, take
 * the whole screen; so does an empty page list. False when a page is shown. */
static bool message_for(const link_info_t *link, int page_count, int64_t now_ms, char *title, char *main,
                        char *hint, size_t n)
{
    if (now_ms < s_toast.until_ms) {
        snprintf(title, n, "Dark mode");
        snprintf(main, n, "%s", s_toast.main);
        snprintf(hint, n, "%s", s_toast.hint);
        return true;
    }
    switch (link->state) {
    case LINK_SETUP:
        snprintf(title, n, "WiFi setup: join");
        snprintf(main, n, "%s", link->portal_ssid);
        snprintf(hint, n, "then open http://%s", link->portal_ip);
        return true;
    case LINK_APPROVE:
        snprintf(title, n, "Signal K");
        snprintf(main, n, "Approve access");
        snprintf(hint, n, "Security > Access Requests");
        return true;
    default:
        break;
    }
    if (page_count == 0) {
        snprintf(title, n, "No pages yet. Open");
        snprintf(main, n, "%s.local", link->hostname);
        snprintf(hint, n, "and add them under Pages");
        return true;
    }
    return false;
}

static link_dot_t dot_for(link_state_t state)
{
    switch (state) {
    case LINK_NO_NETWORK:
        return LINK_DOT_RED;
    case LINK_NO_SK:
        return LINK_DOT_AMBER;
    default:
        return LINK_DOT_NONE;
    }
}

static void runtime_task(void *arg)
{
    (void)arg;
    int last_index = -1;
    dinvalue_slot_t last_slots[DINPAGE_MAX_VALUES] = {0};
    char last_msg[3 * 48] = "";
    int last_dot = -1;
    int last_step = dinmeter_input_get_count() / 2;
    int page_pos = 0;
    bool btn_down = false;
    int btn_stable = 0;
    int64_t press_start_ms = 0;
    bool long_done = false;
    int64_t last_use_ms = esp_timer_get_time() / 1000;
    bool asleep = false;

    for (;;) {
        int64_t now_ms = esp_timer_get_time() / 1000;
        int step = dinmeter_input_get_count() / 2;
        int delta = step - last_step;
        last_step = step;

        bool pressed_now = false;
        bool raw = dinmeter_input_button_pressed();
        btn_stable = raw == btn_down ? 0 : btn_stable + 1;
        if (btn_stable >= DEBOUNCE_POLLS) {
            btn_down = raw;
            btn_stable = 0;
            if (btn_down) {
                pressed_now = true;
                press_start_ms = now_ms;
                long_done = false;
            }
        }
        bool used = delta != 0 || pressed_now;
        if (btn_down && !long_done && now_ms - press_start_ms >= LONG_PRESS_MS) {
            long_done = true;
            used = true;
            if (darkmode_enabled()) {
                darkmode_set(!darkmode_active());
                toast(now_ms, darkmode_active() ? "On" : "Off", darkmode_active() ? "screen sleeps when unused" : "");
            } else {
                toast(now_ms, "Not enabled", "turn it on under Pages > Display");
            }
        }
        if (used) {
            last_use_ms = now_ms;
        }

        /* The turn or press that wakes the screen only wakes it: in the dark
         * nobody sees which page they would have turned to. */
        if (asleep && (used || !darkmode_active())) {
            asleep = false;
            delta = 0;
            dinmeter_display_backlight_on();
        } else if (!asleep && darkmode_active() && now_ms - last_use_ms > (int64_t)darkmode_timeout_ms() &&
                   now_ms >= s_toast.until_ms) {
            asleep = true;
            dinmeter_display_backlight_off();
        }
        page_pos += delta;

        link_info_t link;
        link_state_get(&link);

        xSemaphoreTake(s_lock, portMAX_DELAY);
        int n = s_page_count;
        dinpage_t page_copy = {0};
        dinvalue_slot_t current[DINPAGE_MAX_VALUES] = {0};
        int index = -1;
        if (n > 0) {
            index = ((page_pos % n) + n) % n;
            page_copy = s_pages[index];
            for (int v = 0; v < page_copy.value_count; v++) {
                current[v].state = effective_state(index, v, page_copy.stale_timeout_ms);
                snprintf(current[v].text, sizeof(current[v].text), "%s", s_value_state[index][v].slot.text);
            }
        }
        xSemaphoreGive(s_lock);

        char title[48], main[48], hint[48];
        if (message_for(&link, n, now_ms, title, main, hint, sizeof(title))) {
            char key[sizeof(last_msg)];
            snprintf(key, sizeof(key), "%s\n%s\n%s", title, main, hint);
            if (strcmp(key, last_msg) != 0) {
                renderer_show_message(title, main, hint);
                snprintf(last_msg, sizeof(last_msg), "%s", key);
                last_index = -1; /* redraw the page when it returns */
                last_dot = -1;
            }
        } else {
            last_msg[0] = '\0';
            bool changed = index != last_index;
            for (int v = 0; v < page_copy.value_count && !changed; v++) {
                changed = !slots_equal(&current[v], &last_slots[v]);
            }
            if (changed) {
                if (page_copy.value_count == 2) {
                    renderer_show_dual_page(&page_copy, current);
                } else {
                    renderer_show_page(&page_copy, &current[0]);
                }
                last_index = index;
                for (int v = 0; v < DINPAGE_MAX_VALUES; v++) {
                    last_slots[v] = current[v];
                }
            }
            link_dot_t dot = dot_for(link.state);
            if ((int)dot != last_dot) {
                renderer_set_link_dot(dot);
                last_dot = (int)dot;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t dinmeter_runtime_start(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = renderer_init();
    if (err != ESP_OK) {
        return err;
    }

    dinmeter_pages_on_change(resync_pages_and_subscriptions, NULL);
    resync_pages_and_subscriptions(NULL);

    if (xTaskCreate(runtime_task, "dinmeter_rt", 6144, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
