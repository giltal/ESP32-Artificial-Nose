#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Init ST7789 panel, backlight and LVGL. */
esp_err_t display_init(void);

/* DAC test screen: four channel voltages (2x2), the selected one highlighted. */
void display_show_dac(const uint16_t mv[4], int selected);

/* Title row: green dot + Vref when the MCP4728 is detected, red dot otherwise. */
void display_show_dac_status(bool present, uint16_t vref_mv, bool vref_vdd);

/* The two 0..5V analog inputs, in millivolts. */
void display_show_inputs(const uint16_t mv[2]);
