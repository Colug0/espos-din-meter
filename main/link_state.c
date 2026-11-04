/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
#include "link_state.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_timer.h"
#include "espos_event.h"
#include "espos_net.h"
#include "espos_sk.h"
#include "espos_wifi.h"

static atomic_bool s_sk_up;

static void on_sk_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    atomic_store(&s_sk_up, id == ESPOS_EVENT_SK_STREAM_CONNECTED);
}

esp_err_t link_state_init(void)
{
    esp_err_t err = espos_event_subscribe(ESPOS_EVENT_SK_STREAM_CONNECTED, on_sk_event, NULL);
    if (err == ESP_OK) {
        err = espos_event_subscribe(ESPOS_EVENT_SK_STREAM_DISCONNECTED, on_sk_event, NULL);
    }
    return err;
}

bool link_state_sk_up(void)
{
    return atomic_load(&s_sk_up);
}

/* espOS reports the token state only in its status document. */
static bool token_pending(void)
{
    char *json = NULL;
    if (espos_sk_status_json(&json) != ESP_OK || !json) {
        return false;
    }
    cJSON *root = cJSON_Parse(json);
    free(json);
    const cJSON *state = cJSON_GetObjectItem(cJSON_GetObjectItem(root, "token"), "state");
    bool pending = cJSON_IsString(state) && strcmp(state->valuestring, "pending") == 0;
    cJSON_Delete(root);
    return pending;
}

void link_state_get(link_info_t *out)
{
    static int64_t s_last_token_check_ms;
    static bool s_pending;

    espos_wifi_status_t w = {0};
    espos_wifi_get_status(&w);
    memset(out, 0, sizeof(*out));
    snprintf(out->hostname, sizeof(out->hostname), "%s", w.hostname);

    if (!espos_net_is_up()) {
        s_pending = false;
        if (w.portal_ssid[0]) {
            out->state = LINK_SETUP;
            snprintf(out->portal_ssid, sizeof(out->portal_ssid), "%s", w.portal_ssid);
            snprintf(out->portal_ip, sizeof(out->portal_ip), "%s", w.portal_ip);
        } else {
            out->state = LINK_NO_NETWORK;
        }
        return;
    }
    if (atomic_load(&s_sk_up)) {
        s_pending = false;
        out->state = LINK_OK;
        return;
    }
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_last_token_check_ms >= 2000) {
        s_last_token_check_ms = now_ms;
        s_pending = token_pending();
    }
    out->state = s_pending ? LINK_APPROVE : LINK_NO_SK;
}
