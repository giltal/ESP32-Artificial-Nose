#include "display.h"
#include "board_pins.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "display";

static lv_obj_t *s_dot;
static lv_obj_t *s_title;
static lv_obj_t *s_vref;
static lv_obj_t *s_dac[4];
static lv_obj_t *s_ain[2];

#define ROW_H   26
#define ROW_Y0  28
#define CELL_W  ((LCD_H_RES - 24) / 2)

static lv_obj_t *make_cell(lv_obj_t *parent, int x, int y, int w)
{
    lv_obj_t *cell = lv_label_create(parent);
    lv_obj_set_size(cell, w, ROW_H - 2);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_style_text_font(cell, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_left(cell, 6, 0);
    lv_obj_set_style_radius(cell, 4, 0);
    lv_obj_set_style_bg_color(cell, lv_palette_darken(LV_PALETTE_CYAN, 3), 0);
    return cell;
}

static void build_ui(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    /* Title row: [dot] MCP4728            Vref ... */
    s_dot = lv_obj_create(scr);
    lv_obj_remove_style_all(s_dot);
    lv_obj_set_size(s_dot, 12, 12);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_set_pos(s_dot, 10, 8);

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_label_set_text(s_title, "MCP4728");
    lv_obj_set_pos(s_title, 28, 2);

    s_vref = lv_label_create(scr);
    lv_obj_set_style_text_font(s_vref, &lv_font_montserrat_14, 0);
    lv_obj_align(s_vref, LV_ALIGN_TOP_RIGHT, -10, 7);

    /* DAC channels as a 2x2 grid: A B / C D */
    for (int i = 0; i < 4; i++) {
        s_dac[i] = make_cell(scr, 8 + (i % 2) * (CELL_W + 8), ROW_Y0 + (i / 2) * ROW_H, CELL_W);
    }
    for (int i = 0; i < 2; i++) {
        s_ain[i] = make_cell(scr, 8, ROW_Y0 + (2 + i) * ROW_H, LCD_H_RES - 16);
        lv_obj_set_style_text_color(s_ain[i], lv_palette_main(LV_PALETTE_AMBER), 0);
    }
}

esp_err_t display_init(void)
{
    gpio_config_t bl_cfg = {
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(LCD_PIN_BL, 0);

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
    /* 135x240 visible window inside the ST7789's 240x320 RAM (landscape) */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, 40, 53));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        },
    };
    if (lvgl_port_add_disp(&disp_cfg) == NULL) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_FAIL;
    }

    lvgl_port_lock(0);
    build_ui();
    lvgl_port_unlock();

    gpio_set_level(LCD_PIN_BL, 1);
    ESP_LOGI(TAG, "ST7789 %dx%d ready", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

void display_show_dac_status(bool present, uint16_t vref_mv, bool vref_vdd)
{
    lvgl_port_lock(0);
    lv_obj_set_style_bg_color(s_dot, lv_palette_main(present ? LV_PALETTE_GREEN : LV_PALETTE_RED), 0);
    if (present) {
        unsigned dv = (vref_mv + 50) / 100;  /* decivolts: "Vref 5.0V" */
        lv_label_set_text_fmt(s_vref, "Vref %u.%uV", dv / 10, dv % 10);
        lv_obj_set_style_text_color(s_vref, lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    } else {
        lv_label_set_text(s_vref, "not found");
        lv_obj_set_style_text_color(s_vref, lv_palette_main(LV_PALETTE_RED), 0);
    }
    lvgl_port_unlock();
}

void display_show_dac(const uint16_t mv[4], int selected)
{
    lvgl_port_lock(0);
    for (int i = 0; i < 4; i++) {
        lv_label_set_text_fmt(s_dac[i], "%c %u.%03u", 'A' + i,
                              (unsigned)(mv[i] / 1000), (unsigned)(mv[i] % 1000));
        bool sel = (i == selected);
        lv_obj_set_style_bg_opa(s_dac[i], sel ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_dac[i], sel ? lv_color_white() : lv_palette_lighten(LV_PALETTE_GREY, 1), 0);
    }
    lvgl_port_unlock();
}

void display_show_inputs(const uint16_t mv[2])
{
    lvgl_port_lock(0);
    for (int i = 0; i < 2; i++) {
        lv_label_set_text_fmt(s_ain[i], "IN%d   %u.%02u V", i + 1,
                              (unsigned)(mv[i] / 1000), (unsigned)(mv[i] % 1000 / 10));
    }
    lvgl_port_unlock();
}
