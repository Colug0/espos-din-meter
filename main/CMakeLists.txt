# SPDX-FileCopyrightText: 2026 Thomas Christoffel
# SPDX-License-Identifier: Apache-2.0
#
# espos_ota and espos_sk are listed although app_main.c does not call them:
# espOS consumers build with IDF's MINIMAL_BUILD, so a component nothing
# requires is not compiled, and espos_start() starts only what is in the
# build. Without espos_ota the device could never be updated in the field.
idf_component_register(
    SRCS app_main.c runtime.c renderer.c link_state.c brightness.c
    INCLUDE_DIRS "."
    PRIV_REQUIRES dinmeter_hal dinmeter_pages espos_core espos_event espos_net espos_wifi espos_sk espos_ota espos_config
                  esp_event esp_wifi esp_timer espressif__cjson
                  lvgl__lvgl espressif__esp_lvgl_port)

# The "display" settings namespace (brightness). After idf_component_register.
espos_config_add_descriptor(config/display.json)
