/* SPDX-FileCopyrightText: 2026 Thomas Christoffel */
/* SPDX-License-Identifier: Apache-2.0 */
/*
 * M5Stack DIN Meter (ESP32-S3FN8, 8 MB flash, no PSRAM, 135x240 ST7789,
 * rotary encoder with push button, buzzer). Pins from M5GFX and M5Unified;
 * docs.m5stack.com swaps the button and buzzer pins, the source is right.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* espos_start_opts_t.board and the registry's reportedAs. Never change it:
 * OTA matches on it. */
#define DINMETER_BOARD_NAME "M5Stack DIN Meter"

#define DINMETER_PIN_LCD_MOSI 5
#define DINMETER_PIN_LCD_SCLK 6
#define DINMETER_PIN_LCD_DC 4
#define DINMETER_PIN_LCD_CS 7
#define DINMETER_PIN_LCD_RST 8
#define DINMETER_PIN_LCD_BL 9

#define DINMETER_PIN_ENC_A 41
#define DINMETER_PIN_ENC_B 40
#define DINMETER_PIN_BUTTON 42
#define DINMETER_PIN_BUZZER 3

#ifdef __cplusplus
}
#endif
