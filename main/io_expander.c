#include "io_expander.h"
#include "board_pins.h"

#include "esp_log.h"

static const char *TAG = "pcf8574";

/* PCF8574T base 0x20, PCF8574AT base 0x38; A0=1, A1=A2=0 on the platform */
static const uint8_t k_candidate_addrs[] = { 0x21, 0x39 };

/*
 * Output latch. Pins written 1 are quasi-bidirectional inputs (weak pull-up).
 *  P0 SD_CS      = 1 (deselected)
 *  P1..P3 S1..S3 = 1 (inputs)
 *  P4 M_SLEEP    = 0 (DRV8833 asleep)
 *  P5 IO_EXT_0   = 1
 *  P6/P7 LEDs    = 0 (off)
 */
static uint8_t s_latch = (1 << PCF_BIT_SD_CS) | (1 << PCF_BIT_S1) | (1 << PCF_BIT_S2) |
                         (1 << PCF_BIT_S3) | (1 << PCF_BIT_IO_EXT_0);

static i2c_master_dev_handle_t s_dev;

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_PIN_SDA,
        .scl_io_num = I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, out_bus);
    if (err != ESP_OK) {
        return err;
    }

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(*out_bus, addr, 20) == ESP_OK) {
            ESP_LOGI(TAG, "I2C device found at 0x%02X", addr);
        }
    }
    return ESP_OK;
}

esp_err_t io_expander_init(i2c_master_bus_handle_t bus)
{
    uint8_t addr = 0;
    for (size_t i = 0; i < sizeof(k_candidate_addrs); i++) {
        if (i2c_master_probe(bus, k_candidate_addrs[i], 20) == ESP_OK) {
            addr = k_candidate_addrs[i];
            break;
        }
    }
    if (addr == 0) {
        ESP_LOGE(TAG, "PCF8574 not found at 0x21 or 0x39");
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_master_transmit(s_dev, &s_latch, 1, 50);
    ESP_LOGI(TAG, "PCF8574 at 0x%02X, latch=0x%02X (%s)", addr, s_latch, esp_err_to_name(err));
    return err;
}

esp_err_t io_expander_read(uint8_t *value)
{
    return i2c_master_receive(s_dev, value, 1, 50);
}

esp_err_t io_expander_read_buttons(uint8_t *mask)
{
    uint8_t port;
    esp_err_t err = io_expander_read(&port);
    if (err != ESP_OK) {
        return err;
    }
    *mask = (port >> PCF_BIT_S1) & 0x07;
    return ESP_OK;
}
