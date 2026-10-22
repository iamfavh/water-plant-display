#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    float temperature_c;
    float humidity_pct;
} dht22_reading_t;

esp_err_t dht22_init(gpio_num_t pin);

// Blocks for about 5 ms. The sensor needs at least 2 s between reads.
esp_err_t dht22_read(dht22_reading_t *out);
