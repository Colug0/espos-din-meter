# SPDX-FileCopyrightText: 2026 Thomas Christoffel
# SPDX-License-Identifier: Apache-2.0
idf_component_register(
    SRCS src/display.c src/input.c src/buzzer.c
    INCLUDE_DIRS include
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_driver_pcnt esp_driver_ledc)
