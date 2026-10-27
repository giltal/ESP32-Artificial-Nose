idf_component_register(
    SRCS "main.c" "display.c" "io_expander.c" "mcp4728.c" "analog_in.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_driver_gpio esp_driver_i2c esp_driver_spi esp_adc
)
