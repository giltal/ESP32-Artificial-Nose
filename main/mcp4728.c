#include "mcp4728.h"
#include "board_pins.h"

#include "esp_log.h"

static const char *TAG = "mcp4728";

#define MCP4728_ADDR_FIRST  0x60
#define MCP4728_ADDR_LAST   0x67

#define CMD_MULTI_WRITE     0x40    /* 0100 0 DAC1 DAC0 UDAC — input regs only, no EEPROM */
#define CFG_VREF_VDD        0x00    /* VREF bit = 0: VDD reference (gain bit ignored) */
#define CFG_PD_NORMAL       0x00

static i2c_master_dev_handle_t s_dev;

esp_err_t mcp4728_init(i2c_master_bus_handle_t bus)
{
    if (s_dev) {
        return ESP_OK;
    }

    uint8_t addr = 0;
    for (uint8_t a = MCP4728_ADDR_FIRST; a <= MCP4728_ADDR_LAST; a++) {
        if (i2c_master_probe(bus, a, 20) == ESP_OK) {
            addr = a;
            break;
        }
    }
    if (addr == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        s_dev = NULL;
        return err;
    }
    ESP_LOGI(TAG, "MCP4728 at 0x%02X", addr);
    return ESP_OK;
}

bool mcp4728_present(void)
{
    return s_dev != NULL;
}

static void encode(uint8_t *out, int channel, uint16_t mv)
{
    uint32_t code = ((uint32_t)mv * 4096 + MCP4728_VDD_MV / 2) / MCP4728_VDD_MV;
    if (code > 4095) {
        code = 4095;
    }
    out[0] = CMD_MULTI_WRITE | (channel << 1);  /* UDAC=0: update now */
    out[1] = CFG_VREF_VDD | CFG_PD_NORMAL | (code >> 8);
    out[2] = code & 0xFF;
}

esp_err_t mcp4728_set_mv(int channel, uint16_t mv)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel < 0 || channel >= MCP4728_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buf[3];
    encode(buf, channel, mv);
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}

esp_err_t mcp4728_set_all_mv(const uint16_t mv[MCP4728_CHANNELS])
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t buf[3 * MCP4728_CHANNELS];
    for (int ch = 0; ch < MCP4728_CHANNELS; ch++) {
        encode(&buf[3 * ch], ch, mv[ch]);
    }
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 50);
}

/* 24 bytes: per channel 3 bytes DAC register + 3 bytes EEPROM */
static esp_err_t read_registers(uint8_t buf[24])
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_receive(s_dev, buf, 24, 50);
}

esp_err_t mcp4728_read_mv(uint16_t mv[MCP4728_CHANNELS])
{
    uint8_t buf[24];
    esp_err_t err = read_registers(buf);
    if (err != ESP_OK) {
        return err;
    }
    for (int ch = 0; ch < MCP4728_CHANNELS; ch++) {
        const uint8_t *r = &buf[6 * ch];
        uint32_t code = ((r[1] & 0x0F) << 8) | r[2];
        if (r[1] & 0x80) {
            /* Internal 2.048V ref (e.g. EEPROM default): 0.5 mV x1, 1 mV x2 */
            mv[ch] = (r[1] & 0x10) ? code : code / 2;
        } else {
            mv[ch] = (code * MCP4728_VDD_MV + 2048) / 4096;
        }
    }
    return ESP_OK;
}

esp_err_t mcp4728_read_vref(uint16_t *vref_mv, bool *from_vdd)
{
    uint8_t buf[24];
    esp_err_t err = read_registers(buf);
    if (err != ESP_OK) {
        return err;
    }
    *from_vdd = !(buf[1] & 0x80);
    *vref_mv = *from_vdd ? MCP4728_VDD_MV : ((buf[1] & 0x10) ? 4096 : 2048);
    return ESP_OK;
}
