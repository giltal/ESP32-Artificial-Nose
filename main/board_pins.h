#pragma once

/*
 * Pin map — TTGO T-Display (ESP32) mounted on the ESP32 Course Platform.
 * Sources: Documents/ESP32-TFT(6-26).pdf, Documents/ESP32_CoursePlatformSch.pdf
 */

/* ---- ST7789 1.14" 135x240 TFT (on the T-Display module) ---- */
#define LCD_SPI_HOST        SPI2_HOST
#define LCD_PIN_MOSI        19
#define LCD_PIN_SCLK        18
#define LCD_PIN_CS          5
#define LCD_PIN_DC          16
#define LCD_PIN_RST         23
#define LCD_PIN_BL          4       /* via S8050, active high */
#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
#define LCD_H_RES           240     /* landscape */
#define LCD_V_RES           135

/* ---- I2C bus (course platform: PCF8574, MCP4728) ---- */
#define I2C_PORT            I2C_NUM_0
#define I2C_PIN_SDA         21
#define I2C_PIN_SCL         22      /* 4.7K pull-ups R5/R6 on platform */
#define I2C_FREQ_HZ         100000

/* ---- Analog inputs (0..5V through a resistor divider) ----
 * IO25/IO26 (platform nets DAC25/DAC26). These are ADC2 pins: on the ESP32
 * ADC2 cannot be read while Wi-Fi is running.
 *   5V in --[R_TOP 15K]--+--[R_BOT 12K]-- GND
 *                        +--> IO25 / IO26      (5V -> 2.22V, max 5.5V)  */
#define AIN_ADC_UNIT        ADC_UNIT_2
#define AIN1_ADC_CHANNEL    ADC_CHANNEL_8   /* GPIO25 */
#define AIN2_ADC_CHANNEL    ADC_CHANNEL_9   /* GPIO26 */
#define AIN_R_TOP_OHM       15000
#define AIN_R_BOT_OHM       12000

/* ---- PCF8574 I/O expander (IC1) ----
 * A0=3.3V, A1=GND, A2=GND  -> 0x21 (PCF8574T) or 0x39 (PCF8574AT)
 * INT -> IO39 (input-only pin, external 4.7K pull-up R4)          */
#define PCF8574_PIN_INT     39

#define PCF_BIT_SD_CS       0
#define PCF_BIT_S1          1       /* buttons: pressed = high (4.7K pull-down) */
#define PCF_BIT_S2          2
#define PCF_BIT_S3          3
#define PCF_BIT_M_SLEEP     4       /* DRV8833 nSLEEP */
#define PCF_BIT_IO_EXT_0    5
#define PCF_BIT_RED_LED     6
#define PCF_BIT_YELLOW_LED  7
