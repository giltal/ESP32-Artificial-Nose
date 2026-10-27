/*
 * Artificial Nose — TTGO T-Display + ESP32 Course Platform
 * Step 3: MCP4728 DAC test screen + two 0..5V analog inputs.
 *   S1 = select channel A..D, S2 = -100 mV, S3 = +100 mV (hold to repeat)
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "board_pins.h"
#include "display.h"
#include "io_expander.h"
#include "mcp4728.h"
#include "analog_in.h"

static const char *TAG = "main";

#define BUTTON_POLL_MS      20      /* fallback poll + debounce interval */
#define REPEAT_DELAY_MS     500     /* hold time before auto-repeat (S2/S3) */
#define REPEAT_RATE_MS      100
#define DAC_STEP_MV         100
#define AIN_UPDATE_MS       200     /* analog input refresh */
#define DAC_RETRY_MS        1000    /* re-probe interval while the DAC is missing */

static SemaphoreHandle_t s_int_sem;
static QueueHandle_t s_button_q;    /* int: button number 1..3 */

static void IRAM_ATTR pcf_int_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_int_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

static void button_task(void *arg)
{
    uint8_t stable = 0;
    uint8_t last_raw = 0;
    TickType_t held_since[3] = { 0 };
    TickType_t last_repeat[3] = { 0 };

    for (;;) {
        /* Wake on PCF8574 INT edge, or poll periodically for debouncing */
        xSemaphoreTake(s_int_sem, pdMS_TO_TICKS(BUTTON_POLL_MS));

        uint8_t raw;
        if (io_expander_read_buttons(&raw) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        /* Accept a state only after two consecutive identical reads */
        if (raw != last_raw) {
            last_raw = raw;
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        uint8_t pressed = raw & ~stable;
        stable = raw;
        for (int i = 0; i < 3; i++) {
            int button = i + 1;
            if (pressed & (1 << i)) {
                held_since[i] = last_repeat[i] = now;
                xQueueSend(s_button_q, &button, 0);
            } else if ((stable & (1 << i)) && button != 1 &&
                       now - held_since[i] >= pdMS_TO_TICKS(REPEAT_DELAY_MS) &&
                       now - last_repeat[i] >= pdMS_TO_TICKS(REPEAT_RATE_MS)) {
                last_repeat[i] = now;
                xQueueSend(s_button_q, &button, 0);
            }
        }
    }
}

static void apply_dac(const uint16_t mv[MCP4728_CHANNELS])
{
    esp_err_t err = mcp4728_set_all_mv(mv);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "DAC write failed: %s", esp_err_to_name(err));
        return;
    }
    uint16_t rb[MCP4728_CHANNELS];
    if (mcp4728_read_mv(rb) == ESP_OK) {
        ESP_LOGI(TAG, "DAC readback A=%u B=%u C=%u D=%u mV", rb[0], rb[1], rb[2], rb[3]);
    }
}

static void show_dac_status(void)
{
    uint16_t vref_mv = 0;
    bool vdd = false;
    bool ok = mcp4728_present() && mcp4728_read_vref(&vref_mv, &vdd) == ESP_OK;
    display_show_dac_status(ok, vref_mv, vdd);
}

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());

    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_bus_init(&bus));
    if (io_expander_init(bus) != ESP_OK) {
        ESP_LOGE(TAG, "IO expander missing — buttons disabled");
    }

    bool ain_ok = analog_in_init() == ESP_OK;
    if (!ain_ok) {
        ESP_LOGE(TAG, "ADC init failed — analog inputs disabled");
    }

    uint16_t dac_mv[MCP4728_CHANNELS] = { 0 };
    int selected = 0;

    if (mcp4728_init(bus) == ESP_OK) {
        apply_dac(dac_mv);
    } else {
        ESP_LOGW(TAG, "MCP4728 not found (0x60-0x67), will keep probing");
    }
    show_dac_status();
    display_show_dac(dac_mv, selected);

    s_int_sem = xSemaphoreCreateBinary();
    s_button_q = xQueueCreate(8, sizeof(int));
    gpio_config_t int_cfg = {
        .pin_bit_mask = 1ULL << PCF8574_PIN_INT,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,     /* GPIO39 has no internal pull-up; R4 provides it */
    };
    ESP_ERROR_CHECK(gpio_config(&int_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PCF8574_PIN_INT, pcf_int_isr, NULL));
    xTaskCreate(button_task, "buttons", 3072, NULL, 5, NULL);

    TickType_t last_ain = 0;
    unsigned ain_log_count = 0;     /* log inputs once a second */
    TickType_t last_probe = xTaskGetTickCount();
    for (;;) {
        int button;
        bool got = xQueueReceive(s_button_q, &button, pdMS_TO_TICKS(AIN_UPDATE_MS)) == pdTRUE;
        TickType_t now = xTaskGetTickCount();

        if (ain_ok && now - last_ain >= pdMS_TO_TICKS(AIN_UPDATE_MS)) {
            last_ain = now;
            uint16_t ain_mv[ANALOG_IN_CHANNELS] = { 0 };
            for (int i = 0; i < ANALOG_IN_CHANNELS; i++) {
                analog_in_read_mv(i, &ain_mv[i]);
            }
            display_show_inputs(ain_mv);
            if (++ain_log_count % 5 == 0) {
                ESP_LOGI(TAG, "IN1=%u mV IN2=%u mV", ain_mv[0], ain_mv[1]);
            }
        }

        if (!mcp4728_present() && now - last_probe >= pdMS_TO_TICKS(DAC_RETRY_MS)) {
            last_probe = now;
            if (mcp4728_init(bus) == ESP_OK) {
                apply_dac(dac_mv);
                show_dac_status();
            }
        }

        if (!got) {
            continue;
        }

        uint16_t *v = &dac_mv[selected];
        switch (button) {
        case 1:
            selected = (selected + 1) % MCP4728_CHANNELS;
            break;
        case 2:
            *v = *v > DAC_STEP_MV ? *v - DAC_STEP_MV : 0;
            break;
        case 3:
            *v = *v + DAC_STEP_MV < MCP4728_MAX_MV ? *v + DAC_STEP_MV : MCP4728_MAX_MV;
            break;
        }
        if (button != 1 && mcp4728_present()) {
            apply_dac(dac_mv);
        }
        display_show_dac(dac_mv, selected);
    }
}
