#include "display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

// Wiring: change these to match how the module is connected.
#define PIN_SCLK 4 // module pin "SCL" (SPI clock)
#define PIN_MOSI 5 // module pin "SDA" (SPI data, ESP32 -> display)
#define PIN_CS   22  // module pin "CS"; set to -1 if the module has no CS pin
#define PIN_DC   21 // module pin "DC" / "RS" (low = command, high = data)
#define PIN_RST  18 // module pin "RES" / "RST"
#define PIN_BLK  23  // module pin "BLK" / "BL" (backlight)

#define LCD_HOST    SPI2_HOST
#define LCD_PCLK_HZ (40 * 1000 * 1000)

// LVGL draws into a buffer this many rows tall, then sends it over SPI.
// The whole frame would be 240*240*2 = 115 KB, so it's drawn in strips instead.
#define DRAW_BUF_ROWS 20

static const char *TAG = "display";

esp_err_t display_init(void)
{
    // 1. Backlight GPIO. Off until the panel holds a valid image.
    gpio_config_t bk = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BLK,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bk), TAG, "backlight gpio");
    gpio_set_level(PIN_BLK, 0);

    // 2. SPI bus: the physical wires. MISO is unused because the display is write-only.
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DRAW_BUF_ROWS * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    // 3. Panel IO: how commands and data are framed on the bus (DC pin, 8-bit commands).
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = PIN_CS,
        .dc_gpio_num = PIN_DC,
        .spi_mode = (PIN_CS < 0) ? 3 : 0, // modules without CS expect SPI mode 3
        .pclk_hz = LCD_PCLK_HZ,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io),
                        TAG, "panel io");

    // 4. Panel driver: ST7789-specific command sequences.
    esp_lcd_panel_handle_t panel;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE, // match the ESP32's native uint16_t layout
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io, &panel_cfg, &panel), TAG, "st7789");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "init");
    // Most 240x240 IPS ST7789 panels show inverted colors without this.
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel, true), TAG, "invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel, true), TAG, "display on");

    // 5. LVGL: starts a background task that redraws whatever widgets changed.
    // It allocates its own DMA buffers and waits for each transfer to finish,
    // replacing the strip buffer and semaphore this file used to have.
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl init");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io,
        .panel_handle = panel,
        .buffer_size = DISPLAY_WIDTH * DRAW_BUF_ROWS,
        .double_buffer = true, // draw the next strip while the previous one is sending
        .hres = DISPLAY_WIDTH,
        .vres = DISPLAY_HEIGHT,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
        },
    };
    ESP_RETURN_ON_FALSE(lvgl_port_add_disp(&disp_cfg), ESP_FAIL, TAG, "lvgl display");

    gpio_set_level(PIN_BLK, 1);
    ESP_LOGI(TAG, "ST7789 ready");
    return ESP_OK;
}
