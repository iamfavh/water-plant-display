#include "dht22.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "dht22";

static gpio_num_t s_pin;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

// Waits while the data line stays at `level`. Returns how many microseconds that took,
// or -1 if it didn't change within timeout_us.
static int wait_while_level(int level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(s_pin) == level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return -1;
        }
    }
    return (int)(esp_timer_get_time() - start);
}

esp_err_t dht22_init(gpio_num_t pin)
{
    s_pin = pin;

    // Open-drain: writing 0 pulls the line low, writing 1 lets go so the pull-up
    // (or the sensor) decides the level. That's how both sides share one wire.
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio config");
    gpio_set_level(pin, 1);
    return ESP_OK;
}

esp_err_t dht22_read(dht22_reading_t *out)
{
    uint8_t data[5] = {0};
    esp_err_t err = ESP_OK;

    // Start signal: hold the line low for at least 1 ms (timing here isn't critical).
    gpio_set_level(s_pin, 0);
    esp_rom_delay_us(1200);

    // From here on, pulses are measured in microseconds, so block interrupts
    // that could pause us mid-measurement. No logging allowed inside.
    portENTER_CRITICAL(&s_lock);
    gpio_set_level(s_pin, 1);

    // Sensor response: line goes high briefly, then sensor pulls low ~80 us, high ~80 us.
    if (wait_while_level(1, 100) < 0 ||
        wait_while_level(0, 100) < 0 ||
        wait_while_level(1, 100) < 0) {
        err = ESP_ERR_TIMEOUT;
    }

    // 40 bits, most significant first. Each bit: ~50 us low, then a high pulse
    // whose length is the value: ~27 us = 0, ~70 us = 1.
    for (int i = 0; i < 40 && err == ESP_OK; i++) {
        if (wait_while_level(0, 80) < 0) {
            err = ESP_ERR_TIMEOUT;
            break;
        }
        int high_us = wait_while_level(1, 100);
        if (high_us < 0) {
            err = ESP_ERR_TIMEOUT;
            break;
        }
        data[i / 8] <<= 1;
        if (high_us > 40) {
            data[i / 8] |= 1;
        }
    }
    portEXIT_CRITICAL(&s_lock);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no response from sensor");
        return err;
    }

    // Byte 4 is the low 8 bits of the sum of the other four.
    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) {
        ESP_LOGW(TAG, "checksum mismatch");
        return ESP_ERR_INVALID_CRC;
    }

    // Values arrive as tenths: 0x01 0x9A = 410 -> 41.0. The top bit of temperature is its sign.
    out->humidity_pct = ((data[0] << 8) | data[1]) / 10.0f;
    out->temperature_c = (((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80) {
        out->temperature_c = -out->temperature_c;
    }
    return ESP_OK;
}
