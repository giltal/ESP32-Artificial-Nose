#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

/*
 * MCP4728 quad 12-bit I2C DAC.
 * Configured with VDD as the reference (module powered from 5V):
 *   full scale = VDD, 1 LSB = VDD / 4096 (~1.2 mV). Accuracy follows the 5V rail;
 *   set MCP4728_VDD_MV to the measured supply for best results.
 */
#define MCP4728_CHANNELS    4
#define MCP4728_VDD_MV      5000
#define MCP4728_MAX_MV      (MCP4728_VDD_MV * 4095 / 4096)

/* Probe 0x60..0x67 and attach. Returns ESP_ERR_NOT_FOUND if absent. */
esp_err_t mcp4728_init(i2c_master_bus_handle_t bus);
bool mcp4728_present(void);

/* Set one channel (0=A..3=D) in millivolts; output updates immediately. */
esp_err_t mcp4728_set_mv(int channel, uint16_t mv);

/* Set all four channels in one transaction. */
esp_err_t mcp4728_set_all_mv(const uint16_t mv[MCP4728_CHANNELS]);

/* Read back the DAC output registers (not EEPROM), in millivolts. */
esp_err_t mcp4728_read_mv(uint16_t mv[MCP4728_CHANNELS]);

/* Reference currently set in the chip (channel A register):
 * VDD -> MCP4728_VDD_MV, internal -> 2048 (x1) or 4096 (x2). */
esp_err_t mcp4728_read_vref(uint16_t *vref_mv, bool *from_vdd);
