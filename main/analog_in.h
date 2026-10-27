#pragma once

#include <stdint.h>
#include "esp_err.h"

#define ANALOG_IN_CHANNELS  2

/* Init ADC2 on the two divider inputs (IO25, IO26). */
esp_err_t analog_in_init(void);

/* Averaged input voltage in millivolts at the 5V side of the divider. */
esp_err_t analog_in_read_mv(int input, uint16_t *mv);
