#include "analog_in.h"
#include "board_pins.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "analog_in";

#define SAMPLES     16

static const adc_channel_t k_channels[ANALOG_IN_CHANNELS] = { AIN1_ADC_CHANNEL, AIN2_ADC_CHANNEL };

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

esp_err_t analog_in_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = AIN_ADC_UNIT };
    esp_err_t err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }

    /* 12 dB attenuation: ~150..2450 mV usable on the ESP32 */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    for (int i = 0; i < ANALOG_IN_CHANNELS; i++) {
        err = adc_oneshot_config_channel(s_adc, k_channels[i], &chan_cfg);
        if (err != ESP_OK) {
            return err;
        }
    }

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = AIN_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    err = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(TAG, "ADC%d ready, divider %d/%d ohm", AIN_ADC_UNIT + 1, AIN_R_TOP_OHM, AIN_R_BOT_OHM);
    return ESP_OK;
}

esp_err_t analog_in_read_mv(int input, uint16_t *mv)
{
    if (input < 0 || input >= ANALOG_IN_CHANNELS) {
        return ESP_ERR_INVALID_ARG;
    }

    int sum = 0;
    for (int i = 0; i < SAMPLES; i++) {
        int raw;
        esp_err_t err = adc_oneshot_read(s_adc, k_channels[input], &raw);
        if (err != ESP_OK) {
            return err;
        }
        sum += raw;
    }

    int pin_mv;
    esp_err_t err = adc_cali_raw_to_voltage(s_cali, sum / SAMPLES, &pin_mv);
    if (err != ESP_OK) {
        return err;
    }
    *mv = (uint16_t)((int64_t)pin_mv * (AIN_R_TOP_OHM + AIN_R_BOT_OHM) / AIN_R_BOT_OHM);
    return ESP_OK;
}
