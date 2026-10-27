#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/* Create the shared I2C master bus (also used later by the MCP4728). */
esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus);

/* Probe for the PCF8574 / PCF8574A and set safe output defaults. */
esp_err_t io_expander_init(i2c_master_bus_handle_t bus);

/* Read the 8 port pins. */
esp_err_t io_expander_read(uint8_t *value);

/* Bitmask of pressed buttons: bit0 = S1, bit1 = S2, bit2 = S3. */
esp_err_t io_expander_read_buttons(uint8_t *mask);
